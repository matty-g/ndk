// Cone.cpp

static const char *const CLASS = "Cone";
static const char *const HELP = "Generates a 3D cone";

#include "DDImage/SourceGeo.h"
#include "DDImage/Scene.h"
#include "DDImage/Triangle.h"
#include "DDImage/Knobs.h"
#include "DDImage/Knob.h"
#include "DDImage/Channel3D.h"
#include <math.h>
#include <assert.h>

using namespace DD::Image;

#ifndef mFnStringize
#define mFnStringize2(A) #A
#define mFnStringize(A) mFnStringize2(A)
#endif

#define MAX_CONE_TESSELATION 512

class Cone : public SourceGeo
{
private:
  double radius, height;
  int columns, rows;
  double my_u_extent, my_v_extent;
  bool close_bottom;
  // local matrix that Axis_Knob fills in
  Matrix4 _local;
  bool fix;
  Knob *_pAxisKnob;

protected:
  void _validate(bool for_real)
  {
    // Clamp the mesh size to reasonable numbers:
    columns = MIN(MAX(columns, 3), MAX_CONE_TESSELATION);
    rows = MIN(MAX(rows, 1), MAX_CONE_TESSELATION);
    my_u_extent = clamp(my_u_extent, 0.001, 360.0);
    my_v_extent = clamp(my_v_extent, 0.001, 180.0);
    SourceGeo::_validate(for_real);
  }

public:
  static const Description description;
  const char *Class() const { return CLASS; }
  const char *node_help() const { return HELP; }

  Cone(Node *node) : SourceGeo(node)
  {
    radius = 1.0;
    height = 1.0;
    rows = 1;
    columns = 3;
    close_bottom = true;
    my_u_extent = 360.0;
    my_v_extent = 180.0;
    _local.makeIdentity();
    fix = false;
    _pAxisKnob = NULL;
  }

  void knobs(Knob_Callback f)
  {
    SourceGeo::knobs(f);
    Int_knob(f, &rows, "rows", "rows/columns");
    Tooltip(f, "The maximum rows is " mFnStringize(MAX_CONE_TESSELATION));
    Int_knob(f, &columns, "columns", "");
    Tooltip(f, "The maximum columns is " mFnStringize(MAX_CONE_TESSELATION));
    Double_knob(f, &radius, "radius");
    Double_knob(f, &height, "height");
    Double_knob(f, &my_u_extent, "u_extent", "u extent");
    Double_knob(f, &my_v_extent, "v_extent", "v extent");
    Newline(f);
    Bool_knob(f, &close_bottom, "close_bottom", "close bottom");
    Obsolete_knob(f, "create_uvs", 0);
    Obsolete_knob(f, "create_normals", 0);

    Divider(f);
    // transform knobs
    _pAxisKnob = Axis_knob(f, &_local, "transform");

    if (_pAxisKnob != NULL)
    {
      if (GeoOp::selectable() == true)
        _pAxisKnob->enable();
      else
        _pAxisKnob->disable();
    }

    // This knob is set by knob_default so that all new instances execute
    // the "fix" code, which rotates the cone 180 degrees so that the
    // seam is on the far side from the default camera position.
    Bool_knob(f, &fix, "fix", INVISIBLE);
  }

  /*! The will handle the knob changes.
   */
  int knob_changed(Knob *k)
  {
    if (k != NULL)
    {
      if (k->is("selectable"))
      {
        if (GeoOp::selectable() == true)
          _pAxisKnob->enable();
        else
          _pAxisKnob->disable();
        return 1;
      }
    }

    return SourceGeo::knob_changed(k);
  }

  // Hash up knobs that affect the Cone:
  void get_geometry_hash()
  {
    SourceGeo::get_geometry_hash(); // Get all hashes up-to-date

    // Knobs that change the geometry structure:
    geo_hash[Group_Primitives].append(columns);
    geo_hash[Group_Primitives].append(rows);
    geo_hash[Group_Primitives].append(close_bottom);

    // Knobs that change the point locations:
    geo_hash[Group_Points].append(radius);
    geo_hash[Group_Points].append(height);
    geo_hash[Group_Points].append(columns);
    geo_hash[Group_Points].append(rows);
    geo_hash[Group_Points].append(close_bottom);

    // Knobs that change the vertex attributes:
    geo_hash[Group_Attributes].append(my_u_extent);
    geo_hash[Group_Attributes].append(my_v_extent);

    geo_hash[Group_Matrix].append(_local.a00);
    geo_hash[Group_Matrix].append(_local.a01);
    geo_hash[Group_Matrix].append(_local.a02);
    geo_hash[Group_Matrix].append(_local.a03);

    geo_hash[Group_Matrix].append(_local.a10);
    geo_hash[Group_Matrix].append(_local.a11);
    geo_hash[Group_Matrix].append(_local.a12);
    geo_hash[Group_Matrix].append(_local.a13);

    geo_hash[Group_Matrix].append(_local.a20);
    geo_hash[Group_Matrix].append(_local.a21);
    geo_hash[Group_Matrix].append(_local.a22);
    geo_hash[Group_Matrix].append(_local.a23);

    geo_hash[Group_Matrix].append(_local.a30);
    geo_hash[Group_Matrix].append(_local.a31);
    geo_hash[Group_Matrix].append(_local.a32);
    geo_hash[Group_Matrix].append(_local.a33);
  }

  // Apply the concat matrix to all the GeoInfos.
  void geometry_engine(Scene& scene, GeometryList& out)
  {
    SourceGeo::geometry_engine(scene, out);

    // multiply the node matrix
    for (unsigned i = 0; i < out.size(); i++)
      out[i].matrix = _local * out[i].matrix;
  }

  void create_geometry(Scene& scene, GeometryList& out)
  {
    int obj = 0;

    // std::cout << "rows: " << rows << std::endl;
    // std::cout << "cols: " << columns << std::endl;

    unsigned num_points = 1 + rows * columns + 1; // base + mesh + pinnacle
    // std::cout << "num points: " << num_points << std::endl;

    //=============================================================
    // Build the primitives:
    if (rebuild(Mask_Primitives)) {
      out.delete_objects();
      out.add_object(obj);

      int row_count = rows;

      // Create poly primitives:
      // Bottom endcap:
      int j1 = 1;
      for (int i = 0; i < columns; i++) { 
        int i0 = i % columns; 
        int i1 = (i + 1) % columns; 
        out.add_primitive(obj, new Triangle(0, i1 + j1, i0 + j1));
      }

      while (row_count > 1) {

        for (int j = 0; j < rows+1; j++) {
          int j0 = j * columns + 1; 
          int j1 = (j + 1) * columns + 1;
          for (int i = 0; i < columns; i++) {
            int i0 = i % columns;
            int i1 = (i+1) % columns;  
            out.add_primitive(obj, new Triangle(i0+j0, i1+j0, i0+j1));
            out.add_primitive(obj, new Triangle(i0+j1, i1+j0, i1+j1)); 
          }  
        }
        row_count--;
      }

      // make the triangles to set the top_point
      int top_point = num_points - 1;
      int j0 = 1 + (rows-1) * columns;
      for (int i = 0; i < columns; i++) {
        int i0 = i % columns;
        int i1 = (i+1) % columns;
        out.add_primitive(obj, new Triangle(i0+j0, i1+j0, top_point));
      }

      // Force points and attributes to update:5
      set_rebuild(Mask_Points | Mask_Attributes);
    }

    //=============================================================
    // Create points and assign their coordinates:
    if (rebuild(Mask_Points)) {
      // Generate points:
      PointList* points = out.writable_points(obj);
      points->resize(num_points);

      // Assign the point locations:
      int p = 0;
      // Bottom center:
      (*points)[p++].set(0.0f, 0.0f, 0.0f);

      // Middle mesh:

      int row_count = rows;
      float row_delta = height / float(rows);
      float rad_delta = radius / float(rows);
      int idx = 0;

      while (row_count > 0) {

        float angle = (2.0 * M_PI) / columns;
        float exterior_angle = M_PI - angle;
        float rotation_delta = angle - (0.5 * exterior_angle);

        float y = 0.0 + (row_delta * idx);

        for (int current_angle = 0; current_angle < columns; current_angle++) {
          float new_angle = (angle * current_angle) - rotation_delta;
          float x = cos(new_angle) * (radius - (rad_delta * idx));
          float z = sin(new_angle) * (radius - (rad_delta * idx));
          (*points)[p].set(x, y, z);
          ++p;
        }
        row_count--;
        idx++;
      }

      // set pinnacle
      (*points)[p].set(0.0f, height, 0.0f);
      ++p;

    //=============================================================
    // Assign the normals and uvs:
    if (rebuild(Mask_Attributes)) {
      GeoInfo& info = out[obj];
      //---------------------------------------------
      // NORMALS:
      const Vector3* PNTS = info.point_array();
      Attribute* N = out.writable_attribute(obj, Group_Points, "N", NORMAL_ATTRIB);
      assert(N);
      for (unsigned p = 0; p < num_points; p++)
        N->normal(p) = PNTS[p] / radius;

      //---------------------------------------------
      // UVs:
      const Primitive** PRIMS = info.primitive_array();

      Attribute* uv = out.writable_attribute(obj, Group_Vertices, "uv", VECTOR4_ATTRIB);
      assert(uv);
      float ds = (360.0f / float(my_u_extent)) / float(columns); // U change per column
      float ss = 0.5f - (360.0f / float(my_u_extent)) / 2.0f;     // Starting U
      float dt = (180.0f / float(my_v_extent)) / float(rows);     // V change per row
      float st = 0.5 - (180.0 / my_v_extent) / 2.0;              // Starting V
      float s, t;                                             // Current UV
      t = st;

      int row_count = rows;
      
      // Bottom center:
      s = ss;
      for (int i = 0; i < columns; i++) {
        unsigned v = (*PRIMS++)->vertex_offset();
        uv->vector4(v++).set(   s, 0.0f, 0.0f, 1.0f);
        uv->vector4(v++).set(s + ds, t + dt, 0.0f, 1.0f);
        uv->vector4(v++).set(   s, t + dt, 0.0f, 1.0f);
        s += ds;
      }
      // t += dt;
      t = st;

      // Create the poly mesh in center:

      while (row_count > 1) {

        for (int j = 0; j < (rows - 1); j++) {
          s = ss;
          for (int i = 0; i < columns; i++) {
            unsigned v = (*PRIMS++)->vertex_offset();
            uv->vector4(v++).set(   s,    t, 0.0f, 1.0f);
            uv->vector4(v++).set(s + ds,    t, 0.0f, 1.0f);
            uv->vector4(v++).set(   s, t + dt, 0.0f, 1.0f);
            v = (*PRIMS++)->vertex_offset();
            uv->vector4(v++).set(   s, t + dt, 0.0f, 1.0f);
            uv->vector4(v++).set(s + ds,    t, 0.0f, 1.0f);
            uv->vector4(v++).set(s + ds, t + dt, 0.0f, 1.0f);
            s += ds;
          }
          t += dt;
        }
        row_count--;
      }

      // Top endcap:
      s = ss;
      for (int i = 0; i < columns; i++) {
        unsigned v = (*PRIMS++)->vertex_offset();
        uv->vector4(v++).set(   s,    t, 0.0f, 1.0f);
        uv->vector4(v++).set(s + ds,    t, 0.0f, 1.0f);
        uv->vector4(v++).set(   s, 1.0f, 0.0f, 1.0f);
        s += ds;
      }
    }
  }
}

    // virtual
    void build_handles(ViewerContext * ctx)
    {
      // call build_matrix_handle to multiply the context model matrix with the local matrix so the
      // nodes above it will display correctly
      GeoOp::build_matrix_handles(ctx, &_local);
    }
  };

  static Op *build(Node *node) { return new Cone(node); }
  const Op::Description Cone::description(CLASS, "3D/Geometry/Cone", build);

  // end of Cone.cpp
