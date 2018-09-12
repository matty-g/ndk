//
//  DeepOpacity.cpp
//
//  Created by Matt Greig on 23/04/2018.
//

/* Control the opacity of a Deep Image with a curve */

#include "DDImage/Knobs.h"
#include "DDImage/DeepOp.h"
#include "DDImage/DeepPixelOp.h"
#include "DDImage/DeepFilterOp.h"
#include "DDImage/LookupCurves.h"

using namespace DD::Image;

static const char* const RCLASS = "DeepOpacity";
static const char* const HELP = "Adjust the opacity (alpha) per deep sample.";
static const CurveDescription   lookupCurvesDefaults[]  = { { "master", "y C 1 1" }, {0} }; // FIND OUT HOW TO SET A DEFAULT FLAT CURVE @ 1

class DeepOpacity : public DeepPixelOp
{
  // operates on Alpha channel only
  LookupCurves _lookupCurvesKnob;

public:

  DeepOpacity(Node* node) : DeepPixelOp(node),
  _lookupCurvesKnob(lookupCurvesDefaults)
  {
  }

  virtual Op* default_input(int idx) const
  {
    return NULL;
  }
  DeepOp* input0() {
      return dynamic_cast<DeepOp*>(Op::input(0));
  }
  const char* node_shape() const
  {
    return DeepOp::DeepNodeShape();
  }

  // /*virtual*/
  void getDeepRequests(Box bbox, const DD::Image::ChannelSet& channels, int count, std::vector<RequestData>& requests) 
  {
      DeepPixelOp::getDeepRequests(bbox, channels, count, requests);
  }

  virtual bool doDeepEngine(Box box, const ChannelSet& channels, DeepOutputPlane& outPlane)
  {
    return DeepPixelOp::doDeepEngine(box, channels, outPlane);
  }

  void _validate(bool);
  float lookup(int z, float value) const;
  virtual void knobs(Knob_Callback);
  virtual void processSample(int y, int x, const DeepPixel& deepPixel, size_t sampleNo, const ChannelSet& channels, DeepOutPixel& output) const;
  const char* Class() const { return RCLASS; }
  const char* node_help() const { return HELP; }
  static Iop::Description d;
};

void DeepOpacity::_validate(bool for_real)
{
  DeepFilterOp::_validate(for_real);
}

float DeepOpacity::lookup(int z, float value) const
{
  value = float(_lookupCurvesKnob.getValue(0, value));
  return value;
}

void DeepOpacity::processSample(int y, int x, const DeepPixel& deepPixel, size_t sampleNo, const ChannelSet& channels, DeepOutPixel& output) const {

  float orig_alpha = deepPixel.getUnorderedSample(sampleNo, Chan_Alpha);
  float deep_val = deepPixel.getUnorderedSample(sampleNo, Chan_DeepFront);
  float curve_val = lookup(0, deep_val);
  float mod_alpha = orig_alpha * curve_val;

  foreach(z, channels) {
    if (z == Chan_Red || z == Chan_Green || z == Chan_Blue){
      float unpremult_val = deepPixel.getUnorderedSample(sampleNo, z) / orig_alpha;
      float new_val = unpremult_val * mod_alpha;
      output.push_back(new_val);
    }
    else if (z == Chan_Alpha) {
      output.push_back(mod_alpha);
    }
    else {
      output.push_back(deepPixel.getUnorderedSample(sampleNo, z));
    }
  }
}

static const char* const enums[] = {
  "remove", "keep", 0
};


void DeepOpacity::knobs(Knob_Callback f)
{
  Obsolete_knob(f, "action", "knob operation $value");
  LookupCurves_knob(f, &_lookupCurvesKnob, "LookupCurves_knob"); 
}

static Op* build(Node* node) { return new DeepOpacity(node); }
Op::Description DeepOpacity::d(RCLASS, "Color/DeepOpacity", build);