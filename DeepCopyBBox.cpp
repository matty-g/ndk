// Copyright (c) 2011 The Foundry Visionmongers Ltd.  All Rights Reserved.

#include "DDImage/DeepFilterOp.h"
#include "DDImage/Knobs.h"

static const char* CLASS = "DeepCopyBBox";

using namespace DD::Image;

class DeepCopyBBox : public DeepFilterOp
{
  //
  // float _zrange[2];
  // bool _useZMin;
  // bool _useZMax;
  // bool _outsideZRange;

  float _bbox[4];
  bool _useBBox;
  bool _outsideBBox;

public:

  int minimum_inputs() const { return 2; }
  int maximum_inputs() const { return 2; }

  DeepCopyBBox(Node* node) : DeepFilterOp(node) {
    // _zrange[0] = 1;
    // _zrange[1] = 2;

    _bbox[0] = _bbox[2] = input_format().width();
    _bbox[1] = _bbox[3] = input_format().height();

    _bbox[0] *= 0.2;
    _bbox[1] *= 0.2;

    _bbox[2] *= 0.8;
    _bbox[3] *= 0.8;

    // _useZMin = true;
    // _useZMax = true;
    _useBBox = true;

    // _outsideZRange = false;
    _outsideBBox = false;
  }

  const char* input_label(int n, char*) const
  {
    switch (n) {
      case 0: return "Deep_in";
      case 1: return "BBox_in";
      default: return "mask";
    }
  }

  const char* node_help() const
  {
    return "Copy a BBox from an input to this Deep Data Stream";
  }

  const char* Class() const {
    return CLASS;
  }

  virtual Op* op()
  {
    return this;
  }

  void knobs(Knob_Callback f)
  {
    // Float_knob(f, &_zrange[0], "znear");
    // SetRange(f, 0, 1);
    // Tooltip(f, "The near deep value");

    // Bool_knob(f, &_useZMin, "use_znear", "use");
    // Tooltip(f, "Whether to use the near deep value");
    //
    // Float_knob(f, &_zrange[1], "zfar");
    // SetRange(f, 0, 1);
    // Tooltip(f, "The far deep value");
    //
    // Bool_knob(f, &_useZMax, "use_zfar", "use");
    // Tooltip(f, "Whether to use the far deep value");
    //
    // Bool_knob(f, &_outsideZRange, "outside_zrange", "keep outside zrange");
    // Tooltip(f, "Whether to keep the samples with deep between the range (false), or outside the range (true)");

    BBox_knob(f, &_bbox[0], "bbox");
    SetFlags(f, Knob::ALWAYS_SAVE);
    Tooltip(f, "2D bounding box for clipping");

    Bool_knob(f, &_useBBox, "use_bbox", "use");
    Tooltip(f, "Whether to use the 2D bounding box");

    Bool_knob(f, &_outsideBBox, "outside_bbox", "keep outside bbox");
    Tooltip(f, "Whether to keep samples within the 2D bounding box (false), or outside it (true)");
  }

  int knob_changed(DD::Image::Knob* k)
  {
    // knob("znear")->enable(_useZMin);
    // knob("zfar")->enable(_useZMax);
    knob("bbox")->enable(_useBBox);
    return 1;
  }

  // void _request(int x, int y, int r, int t, ChannelMask channels, int count)
  // {
  //
  //
  //
  //
  // }

  void _validate(bool for_real)
  {
    _deepInfo = dynamic_cast<DeepOp*>(Op::input(1))->deepInfo();

    // input(1)->request( x, y, r, t, channels, count );
    _bbox[0] = _deepInfo.x();
    _bbox[1] = _deepInfo.r();
    _bbox[2] = _deepInfo.t();
    _bbox[3] = _deepInfo.y();

    DeepFilterOp::_validate(for_real);
    if (_useBBox && !_outsideBBox) {
      _deepInfo.box().set(floor(_bbox[0] - 1), floor(_bbox[1] - 1), ceil(_bbox[2] + 1), ceil(_bbox[3] + 1));
    }
  }

  bool doDeepEngine(DD::Image::Box box, const ChannelSet& channels, DeepOutputPlane& plane)
  {
    // if (!input0())
    //   return true;

    DeepOp* in = input0();
    DeepPlane inPlane;

    ChannelSet needed = channels;
    needed += Mask_DeepFront;

    if (!in->deepEngine(box, needed, inPlane))
      return false;

    plane = DeepOutputPlane(channels, box);

    for (DD::Image::Box::iterator it = box.begin(); it != box.end(); it++) {

      const int x = it.x;
      const int y = it.y;

      bool inXY = (x >= _bbox[0] && x < _bbox[2] && y >= _bbox[1] && y < _bbox[3]);

      if (_useBBox && inXY == _outsideBBox) {
        plane.addHole();
        continue;
      }

      DeepPixel pixel = inPlane.getPixel(it);

      DeepOutPixel pels(pixel.getSampleCount() * channels.size());

      for (size_t sample = 0; sample < pixel.getSampleCount(); sample++) {

        float z = pixel.getUnorderedSample(sample, DD::Image::Chan_DeepFront);

        // bool inZ = (!_useZMax || z <= _zrange[1]) && (!_useZMin || z >= _zrange[0]);
        //
        // if ((_useZMin || _useZMax) && inZ == _outsideZRange)
        //   continue;

        foreach (channel, channels) {
          pels.push_back(pixel.getUnorderedSample(sample, channel));
        }
      }

      plane.addPixel(pels);
    }

    return true;
  }

  static const Description d;
  static const Description d2;
  static const Description d3;
};

static Op* build(Node* node) { return new DeepCopyBBox(node); }
const Op::Description DeepCopyBBox::d(::CLASS, "Image/DeepCopyBBox", build);
const Op::Description DeepCopyBBox::d2("DeepMask", "Image/DeepCopyBBox", build);
const Op::Description DeepCopyBBox::d3("DeepClip", "Image/DeepCopyBBox", build);
