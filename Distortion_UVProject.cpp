// Distortion_UVProject.cpp
// by Matt Greig  mgreig@cuttingedge.com.au
// 2017-07-21

// In honour of the labours of Ryan Coates and in the hope that
// no one will have to endure the sufferings of iRide cleanup 
// the way he did

// Projects UV's to vertices through an equisolid-angle fisheye projection

// Using LDPK_1.10.1 from 3DE 

// Built upon UVProject.cpp from The Foundry Nuke NDK

#include "DDImage/GeoOp.h"
#include "DDImage/Scene.h"
#include "DDImage/CameraOp.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "DDImage/Mesh.h"
#include "DDImage/Channel3D.h"
#include "DDImage/ViewFrustum.h"

#include "DDImage/CameraOp.h"
#include <ldpk/tde4_ld_plugin.h>
#include <ldpk/tde4_ldp_radial_deg_8.h> // init the specific radial fisheye type we want
#include <ldpk/nuke_ld_3de4_base.h> 
#include <ldpk/ldpk_ldp_builtin.h>

#include <assert.h>
#include <typeinfo>

using namespace DD::Image;

static const char* const CLASS = "Distortion_UVProject";
static const char* const HELP = "Project UVs onto vertices via ESA fisheye projection.";

// Point/Vertex:
enum { POINT_LIST = 0, VERTEX_LIST };
static const char* const object_types[] = {
  "points", "vertices", 0
};

// Projection type:
enum {
  OFF = 0, PERSPECTIVE, PLANAR, SPHERICAL, CYLINDRICAL
};
static const char* const proj_types[] = {
  "off", "perspective", "planar", "spherical", "cylindrical", 0
};

enum {
  eBoth = 0, 
  eFront, 
  eBack
};

static const char* const SurfaceNames[] = {
  "both", "front", "back", 0
};

class Distortion_UVProject : public GeoOp
{
private:
  int projection;
  double u_scale, v_scale;
  bool u_invert, v_invert;
  int plane;
  Matrix4 xform;
  Matrix4 projectxform;
  Matrix4 _perspxform;
  const char* uv_attrib_name;
  float inv_u_scale, inv_v_scale;

  bool _frustumCulling;   // if true don't affect the uv coordinate outside the frustum
  int _surface;           // surface to project onto
  Vector3 _cam_dir;       // projection vector

  typedef tde4_ld_plugin ldm_type;
  tde4_ld_plugin* _ldm;

  // some values that 3DE will use
  double _xa_fov_unit,_ya_fov_unit;
	double _xb_fov_unit,_yb_fov_unit;
	double _xd_fov_unit,_yd_fov_unit;
	double _inv_xd_fov_unit,_inv_yd_fov_unit;

  double _tde4_focal_length_cm;
  double _tde4_custom_focus_distance_cm;
  double _tde4_filmback_width_cm;
  double _tde4_filmback_height_cm;
  double _tde4_lens_center_offset_x_cm;
  double _tde4_lens_center_offset_y_cm;
  double _tde4_pixel_aspect;

  // vars specific to Radial_deg_8
  double _Distortion_Degree_2;
  double _Quartic_Distortion_Degree;
  double _Degree_6;
  double _Degree_8;

  double* _values_double;

  // distort/undistort process
  bool _distort;
  bool _worldspace;

protected:
  void _validate(bool for_real)
  {
    // Validate the inputs:
    input0()->validate(for_real);

    // Check if input 1 is connected and get the camera xform from it
    Op* op = Op::input(1);
    if (dynamic_cast<CameraOp*>(op)) {
      op->validate(for_real);
      CameraOp* cam = (CameraOp*)op;
      projectxform.translation(0.5f, 0.5f, 0.0f);
      projectxform.scale(0.5f, static_cast<float>(cam->film_width() / cam->film_height()) * 0.5f, 1.0f);
      projectxform *= cam->projection();
      xform = cam->imatrix();
      _cam_dir = cam->matrix().z_axis();

    }
    else if (dynamic_cast<AxisOp*>(op)) {
      op->validate(for_real);
      xform = ((AxisOp*)op)->imatrix();
      projectxform.makeIdentity();
      _cam_dir = -((AxisOp*)op)->matrix().z_axis();
    }
    else {
      xform.makeIdentity();
      projectxform.makeIdentity();
    }

    inv_u_scale = (float)(1.0 / u_scale);
    inv_v_scale = (float)(1.0 / v_scale);

    // Calculate the geometry hashes:
    GeoOp::_validate(for_real);

    // Calculate FOV quantities
    _xd_fov_unit = _xb_fov_unit - _xa_fov_unit;
    _yd_fov_unit = _yb_fov_unit - _ya_fov_unit;
    _inv_xd_fov_unit = 1.0 / _xd_fov_unit;
    _inv_yd_fov_unit = 1.0 / _yd_fov_unit;

    // copy distortion params into the model
    _ldm->setParameterValue("tde4_focal_length_cm",_tde4_focal_length_cm);
    _ldm->setParameterValue("tde4_custom_focus_distance_cm",_tde4_custom_focus_distance_cm);
    _ldm->setParameterValue("tde4_filmback_width_cm",_tde4_filmback_width_cm);
    _ldm->setParameterValue("tde4_filmback_height_cm",_tde4_filmback_height_cm);
    _ldm->setParameterValue("tde4_lens_center_offset_x_cm",_tde4_lens_center_offset_x_cm);
    _ldm->setParameterValue("tde4_lens_center_offset_y_cm",_tde4_lens_center_offset_y_cm);
    _ldm->setParameterValue("tde4_pixel_aspect",_tde4_pixel_aspect);

    // TODO: are these getting set properly?
    _ldm->setParameterValue("Distortion - Degree 2", _Distortion_Degree_2);
    _ldm->setParameterValue("Quartic Distortion - Degree 4", _Quartic_Distortion_Degree);
    _ldm->setParameterValue("Degree 6", _Degree_6);
    _ldm->setParameterValue("Degree 8", _Degree_8);

    _ldm->initializeParameters();
  }

public:
  static const Description description;
  const char* Class() const
  {
    return CLASS;
  }
  const char* node_help() const
  {
    return HELP;
  }

  void display_vector(std::string name, Vector4& vec) const {
    std::cout << "Vector " << name << ": " << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << std::endl;
  }

  void display_vector(std::string name, Vector3& vec) const {
    std::cout << "Vector " << name << ": " << vec.x << ", " << vec.y << ", " << vec.z << std::endl;
  }

  Distortion_UVProject(Node* node) : GeoOp(node)
  {
    projection = PERSPECTIVE;
    u_scale = v_scale = 1.0;
    u_invert = v_invert = false;
    plane = PLANE_XY;
    uv_attrib_name = "uv";
    _surface = eBoth;
    _frustumCulling = false;

    _ldm = new tde4_ldp_radial_deg_8<ldpk::vec2d,ldpk::mat2d>();
    set_default_values();
    
  }

  int minimum_inputs() const
  {
    return 2;
  }
  int maximum_inputs() const
  {
    return 2;
  }

  bool test_input(int input, Op* op) const
  {
    if (input == 1)
      return dynamic_cast<AxisOp*>(op) != 0;
    return GeoOp::test_input(input, op);
  }

  Op* default_input(int input) const
  {
    if (input == 1)
      return 0;
    return GeoOp::default_input(input);
  }

  const char* input_label(int input, char* buffer) const
  {
    switch (input) {
      case 0:
        return 0;
      case 1:
        return "axis/cam";
      default:
        return 0;
    }
  }

  void knobs(Knob_Callback f)
  {
    GeoOp::knobs(f);
    Enumeration_knob(f, &projection, proj_types, "projection", "projection");
    Obsolete_knob(f, "destination", 0);
    Enumeration_knob(f, &plane, plane_orientation_modes, "plane", "plane");
    Enumeration_knob(f, &_surface, SurfaceNames, "project_on", "project on");
    Bool_knob(f, &_frustumCulling, "frustum_culling", "view frustum culling");
    Tooltip(f, "Project uv onto points and vertices only inside the view frustum.");

    Bool_knob(f, &u_invert, "u_invert", "invert u");
    Bool_knob(f, &v_invert, "v_invert", "invert v");
    Double_knob(f, &u_scale, IRange(0, 2), "u_scale", "u scale");
    Double_knob(f, &v_scale, IRange(0, 2), "v_scale", "v scale");
    String_knob(f, &uv_attrib_name, "uv_attrib_name", "attrib name");

    // DEBUG("Adding custom knobs");
    Tab_knob(f, "3DE Distortion");
    Text_knob(f, "Apply distortion as per the 3DE distortion model.");
    Divider(f, 0);

    // now load in the 3DE-relevant knobs

    // Field-of-View
    Double_knob(f,&_xa_fov_unit,DD::Image::IRange(0.0,1.0),"field_of_view_xa_unit","fov left [unit coord]");
    Double_knob(f,&_ya_fov_unit,DD::Image::IRange(0.0,1.0),"field_of_view_ya_unit","fov bottom [unit coord]");
    Double_knob(f,&_xb_fov_unit,DD::Image::IRange(0.0,1.0),"field_of_view_xb_unit","fov right [unit coord]");
    Double_knob(f,&_yb_fov_unit,DD::Image::IRange(0.0,1.0),"field_of_view_yb_unit","fov top [unit coord]");

    // 3DE built-in parameters
    Double_knob(f,&_tde4_focal_length_cm,DD::Image::IRange(0.5,50.0),"tde4_focal_length_cm","tde4 focal length [cm]");
    Double_knob(f,&_tde4_custom_focus_distance_cm,DD::Image::IRange(10.0,1000.0),"tde4_custom_focus_distance_cm","tde4 focus distance [cm]");
    Double_knob(f,&_tde4_filmback_width_cm,DD::Image::IRange(.1,10.0),"tde4_filmback_width_cm","tde4 filmback width [cm]");
    Double_knob(f,&_tde4_filmback_height_cm,DD::Image::IRange(.1,10.0),"tde4_filmback_height_cm","tde4 filmback height [cm]");
    Double_knob(f,&_tde4_lens_center_offset_x_cm,DD::Image::IRange(-5.0,5.0),"tde4_lens_center_offset_x_cm","tde4 lens center offset x [cm]");
    Double_knob(f,&_tde4_lens_center_offset_y_cm,DD::Image::IRange(-5.0,5.0),"tde4_lens_center_offset_y_cm","tde4 lens center offset y [cm]");
    Double_knob(f,&_tde4_pixel_aspect,DD::Image::IRange(0.25,4.0),"tde4_pixel_aspect","tde4 pixel aspect");

    // Radial_deg_8 params
    Double_knob(f,&_Distortion_Degree_2,DD::Image::IRange(-0.5,0.5),"Distortion_Degree_2","Distortion - Degree 2");
    Double_knob(f,&_Quartic_Distortion_Degree,DD::Image::IRange(-0.5,0.5),"Quartic_Distortion_Degree","Quartic Distortion - Degree 4");
    Double_knob(f,&_Degree_6,DD::Image::IRange(-0.5,0.5),"Degree_6","Degree 6");
    Double_knob(f,&_Degree_8,DD::Image::IRange(-0.5,0.5),"Degree_8","Degree 8");

    // Distort/Undistort
    Bool_knob(f, &_distort, "Distort");
    Bool_knob(f, &_worldspace, "World space");
  }

  void set_default_values() {
      // DEBUG("calling set_default_values()");

      _xa_fov_unit = 0.0;
      _ya_fov_unit = 0.0;
      _xb_fov_unit = 1.0;
      _yb_fov_unit = 1.0;

      // defaulting to iRide camera specs (for now)
      _tde4_focal_length_cm = 8.0;
      _tde4_custom_focus_distance_cm = 100.0;
      _tde4_filmback_width_cm = 2.37402;
      _tde4_filmback_height_cm = 1.57868;
      _tde4_lens_center_offset_x_cm = 0.0;
      _tde4_lens_center_offset_y_cm = 0.0;
      _tde4_pixel_aspect = 1.0;

      _Distortion_Degree_2 = 0.0;
      _Quartic_Distortion_Degree = 0.0;
      _Degree_6 = 0.0;
      _Degree_8 = 0.0;

      _distort = 1;
      _worldspace = 0;

  }

  int knob_changed(Knob* k)
  {
    knob("plane")->enable(projection > PERSPECTIVE);
    knob("u_scale")->enable(projection > PERSPECTIVE);
    knob("v_scale")->enable(projection > PERSPECTIVE);
    knob("frustum_culling")->enable(projection == PERSPECTIVE);
    return 1;
  }

  /*! Hash up knobs that affect the primitive attributes. */
  void get_geometry_hash()
  {
    // Get all hashes up-to-date
    GeoOp::get_geometry_hash();
    if (projection == OFF)
      return;
    // Hash up knobs that affect the UV attributes
    Hash knob_hash;
    knob_hash.reset();
    // Take transform into account:
    xform.append(knob_hash);
    // Take projection matrix into consideration if perspective mode
    if (projection == PERSPECTIVE)
      projectxform.append(knob_hash);
    // Hash rest of local knobs:
    knob_hash.append(projection);
    knob_hash.append(plane);
    knob_hash.append(u_invert);
    knob_hash.append(v_invert);
    knob_hash.append(u_scale);
    knob_hash.append(v_scale);
    knob_hash.append(uv_attrib_name);
    knob_hash.append(_surface);
    knob_hash.append(_frustumCulling);
    // Take point hash into account to force uv upating when positions change
    knob_hash.append(geo_hash[Group_Points]);
    knob_hash.append(geo_hash[Group_Matrix]);
    
    // Change the point or vertex attributes hash:
    geo_hash[Group_Attributes].append(knob_hash);
  }

  inline bool normal_projection_pass_test(Vector3& n) const
  {
    float a = _cam_dir.dot( n );

    if (_surface == eFront){      
      if (a < 0) return false;
    }
    else if (_surface == eBack){
      if (a > 0) return false;
    }

    return true;
  }

  // get transformation from geoinfo local to camera
  Matrix4 getLocalToCameraMatrix(const GeoInfo& info)
  {
    Matrix4 m;
    if (info.matrix == Matrix4::identity())
      m = xform;
    else
      m = xform * info.matrix;
    return m;
  }

  void projection_on_polygon(const AttributePtr norm, 
                             const AttributePtr src, 
                             Attribute* dst, 
                             GeoInfo& info, 
                             DD::Image::GroupType t_group_type,
                             DD::Image::GroupType n_group_type )
  {
    const unsigned num_prims = info.primitives();

    // build projection matrix in local camera coordinate
    _perspxform = projectxform * getLocalToCameraMatrix(info);

    // update frustum
    CameraOp* cam = static_cast<CameraOp*>(Op::input(1));
    // Distortion_Camera2* dist_cam = dynamic_cast<Distortion_Camera2*>(Op::input(1));

    // build camera view frustum in geometry local coordinate
    ViewFrustum frustum;   
    if (_frustumCulling){
      frustum.update(*cam, info.matrix.inverse() * cam->matrix());
    }

    // transform all point in world coordinate   TODO: CHECK THIS OUT <- THIS COULD BE INTERESTING!
    const Vector3* PNTS = info.point_array();

    // vertex index array
    std::vector<unsigned> varray;

    int vertexCount = 0;  // Keep track of the cumulative number of vertices to use as a vertex offset
                          // for PolyMeshes (see comment below).
    for (unsigned p = 0; p < num_prims; ++p) {
      // get primitive
      const Primitive* prim = info.primitive(p);
      const unsigned num_faces = prim->faces();



      // check all faces in the primitive
      for (unsigned f = 0; f < num_faces; f++){
        // get the number of vertex used by face f
        const unsigned num_verts = prim->face_vertices(f);

        // check polygon against frustum
        if (_frustumCulling){
          // skip polygon
          if (frustum.getVisibility(PNTS, prim, f ) == eNotVisible )
            continue;
        }

        // get all used vertices index
        varray.resize(num_verts);

        if (num_faces == 1){
          for (unsigned i = 0; i < num_verts; i++)  {
            varray[i] = prim->vertex(i);          
          }
        }
        else{
          prim->get_face_vertices(f, &varray[0]);

          // As with the num_faces == 1 case, we need vertex indices into the object's points list
          // rather than indices into the face's point list. Do the conversion here.
          if (prim->getPrimitiveType() == ePolyMesh)  {
            for (unsigned i = 0; i < num_verts; i++)  {
              varray[i] = prim->vertex(varray[i]);     
            }
          }
        }

        // check all vertices in the face
        for (unsigned v = 0; v < num_verts; ++v){
          if (f == 50){
          }
          // TODO: vertex_offset is always 0 for polymeshes, since the offset is dependent upon the face in this case
          // (each subface can have a differing number of vertices). For all other primitive type, the offset is fixed 
          // per primitive.
          // Really the Primitive class needs extending to take this into account (vertex_offset() could take a face index 
          // to achieve this). For now fix this here by using a locally calculated offset.
          mFnAssert( (prim->getPrimitiveType() != ePolyMesh) || (prim->vertex_offset() == 0) );
          const unsigned int vertexOffset = (prim->getPrimitiveType() == ePolyMesh) ? vertexCount : prim->vertex_offset();

          unsigned vi = varray[v];      // if (dynamic_cast<Distortion_Camera2*>(op)){
          unsigned ti = (t_group_type == Group_Points) ? vi : (vertexOffset + v);
          unsigned ni = (n_group_type == Group_Points) ? vi : (vertexOffset + v);

          // avoid to re-test the same vertex again
          if (dst->vector4(ti) != src->vector4(ti))
            continue;

          // get vertex normal
          Vector3 n = norm->normal(ni);

          // rotate normal according world info matrix
          n = info.matrix.vtransform(n);

          // avoid to change the uv coordinate associated with the vertex
          if (normal_projection_pass_test(n)) {
            Vector3 mg_uv = PNTS[vi];
            project_point(mg_uv, dst->vector4(ti));
          }
        }

        vertexCount += num_verts;
      }
    }
  }

    // Hijacking 3DE LDPK methods here... FOR NOW      TODO: source these from LDPK directly
    // Map x-coordinate from unit cordinates to fov coordinates.
	double map_in_fov_x(double x_unit) const
		{ return  (x_unit - _xa_fov_unit) * _inv_xd_fov_unit; }
    // Map y-coordinate from unit cordinates to fov coordinates.
	double map_in_fov_y(double y_unit) const
		{ return  (y_unit - _ya_fov_unit) * _inv_yd_fov_unit; }
    // Map x-coordinate from fov cordinates to unit coordinates.
	double map_out_fov_x(double x_fov) const
		{ return x_fov * _xd_fov_unit + _xa_fov_unit; }
    // Map y-coordinate from fov cordinates to unit coordinates.
	double map_out_fov_y(double y_fov) const
		{ return y_fov * _yd_fov_unit + _ya_fov_unit; }






  void undistort_3de(Vector4& pt) {

      Vector3 vec_div = pt.divide_w();

      const double factor = 1.0;

      Vector2 uv(vec_div.x, vec_div.y);

      double xd, yd;

      bool result;

      if (_distort) { 
        result = _ldm->distort(map_in_fov_x(uv.x) / factor, map_in_fov_y(uv.y) / factor, xd, yd);
      } else {
        result = _ldm->undistort(map_in_fov_x(uv.x) / factor, map_in_fov_y(uv.y) / factor, xd, yd);
      }
      double px = map_out_fov_x(xd) * factor;
      double py = map_out_fov_y(yd) * factor;

      Vector3 tmp_vec = Vector3(px, py, vec_div.z);

      Vector4 out_vec = Vector4(tmp_vec, 1);

      pt = out_vec;

      // pt.x = px;
      // pt.y = py;


  }



  void project_point_perspective(int index, GeoInfo& info, GeometryList& out)
  {
    // get normal information
    const AttribContext* N_ref = info.get_attribcontext("N");
    const AttributePtr norm = N_ref ? N_ref->attribute : AttributePtr();

    if(!norm){
      Op::error( "Missing \"N\" channel from geometry");
      return;
    }

    // get the original uv attribute used to restore untouched uv coordinate
    const AttribContext* context = info.get_attribcontext(uv_attrib_name);
    AttributePtr uv_original = context ? context->attribute : AttributePtr();

    if(!uv_original){
      Op::error( "Missing \"%s\" channel from geometry", uv_attrib_name );
      return;
    }

    // we have two possibility:
    // the uv coordinate are stored in Group_Points or in Group_Vertices way
    // the same for normal
    DD::Image::GroupType t_group_type = context->group;  // texture coordinate group type
    DD::Image::GroupType n_group_type = N_ref->group;    // normal group type

    // sanity check
    assert(t_group_type == Group_Points || t_group_type == Group_Vertices);
    assert(n_group_type == Group_Points || n_group_type == Group_Vertices);

    // create a buffer to write on it
    Attribute* uv = out.writable_attribute(index, t_group_type, uv_attrib_name, VECTOR4_ATTRIB);
    assert(uv);

    // copy all original texture coordinate if available
    if (uv_original){

      // sanity check
      assert(uv->size() == uv_original->size());

      for (unsigned i = 0; i < uv->size(); i++)
        uv->vector4(i) =  uv_original->vector4(i);
    }

    // do the projection on all polygons
    projection_on_polygon( norm, uv_original, uv, info, t_group_type, n_group_type );

  }

  /*! Assign UV attribute to point or vertex attribute list. */
  void geometry_engine(Scene& scene, GeometryList& out)
  {
    input0()->get_geometry(scene, out);
    if (projection == OFF)
      return;

    // the perspective projection need a valid
    if ((projection == PERSPECTIVE) && (Op::input(1)==NULL))
      return;

    // Call the engine on all the caches:
    for (unsigned i = 0; i < out.objects(); i++) {
      GeoInfo& info = out[i];

      if (projection == PERSPECTIVE){
        project_point_perspective( i, info, out );
      }
      else
      {
        // get world transformation
        Matrix4 m = getLocalToCameraMatrix(info);

        // Remove UV vertex attribute, as this takes precedence over a point attribute
        info.delete_group_attribute(Group_Vertices, uv_attrib_name, VECTOR4_ATTRIB);
        // Create a point attribute
        Attribute* uv = out.writable_attribute(i, Group_Points, uv_attrib_name, VECTOR4_ATTRIB);
        assert(uv);

        // Project point location and save in UV attribute
        const Vector3* PNTS = info.point_array();
        for (unsigned p = 0; p < info.points(); p++)
          // TODO: manipulate this vertex a la the camera distort??? before transforming?
          project_point(m.transform(*PNTS++), uv->vector4(p));
      }
    }
  }

  void project_point(const Vector3& in, Vector4& out);
};

#define M_TWOPI M_PI * 2.0
#define DEG2RAD M_PI / 180.0
#define RAD2DEG 180.0 / M_PI

/*! Take the point location and project it back through the camera.
    Where it ends up in the camera aperture is the UV coordinate.
 */

 // TODO: OR MAYBE TRY CREATING CUSTOM PROJECTION WHICH USES LDPK A LA CAMERA INPUT
void Distortion_UVProject::project_point(const Vector3& in, Vector4& out)
{
  float a, b, z;
  Vector4 tmp_uv;
  Vector3 tmp;

  switch (projection) {
    default:
    case PERSPECTIVE:
      out = _perspxform.transform(in, 1);
      undistort_3de(out);      
      break;
    case PLANAR:
      switch (plane) {
        case PLANE_XY:
          a = in.x;
          b = in.y;
          break;
        case PLANE_YZ:
          a = in.z;
          b = in.y;
          break;
        case PLANE_ZX:
          a = in.x;
          b = in.z;
          break;
      }
      out.set(a * inv_u_scale + 0.5f, b * inv_v_scale + 0.5f, 0, 1);
      break;
    case SPHERICAL: {
      // latitude
      double phi = acos(-in.y);
      // longitude
      double theta = -atan2(-in.x, in.z);
      // Right side
      if (theta <= 0.0)
        theta += M_TWOPI;
      out.set(static_cast<float>(theta / M_TWOPI) * 0.25f * inv_u_scale, static_cast<float>(phi / M_PI - 0.5) * inv_v_scale + 0.5f, 0, 1);
      break;
    }
    case CYLINDRICAL: {
      // longitude
      double theta = -atan2(-in.x, in.z);
      // Right side
      if (theta <= 0.0)
        theta += M_TWOPI;
      out.set(static_cast<float>(theta / M_TWOPI) * 0.25f * inv_u_scale, in.y * 0.5f * inv_v_scale + 0.5f, 0, 1);
      break;
    }
  }
  if (u_invert)
    out.x = out.w - out.x;
  if (v_invert)
    out.y = out.w - out.y;
}



static Op* build(Node* node)
{
  return new Distortion_UVProject(node);
}
const Op::Description Distortion_UVProject::description(CLASS, build);

// end of UVProject.C
