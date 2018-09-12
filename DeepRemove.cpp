//
//  DeepRemove.cpp
//  DeepRemove Node for Nuke
//
//  Created by Matt Greig on 10/04/2018.
//
//  updated:
//  2018-06-6
//    fixed error where 'Keep' would remove the Deep channel and the node would not function any longer
//

static const char* const RCLASS = "DeepRemove";

static const char* const HELP = "Removes channels from a Deep image.";

/* Remove channels from a Deep input. */

#include "DDImage/Iop.h"
#include "DDImage/NoIop.h"
#include "DDImage/Knobs.h"
#include "DDImage/DeepOp.h"
#include "DDImage/DeepComposite.h"
#include "DDImage/Pixel.h"
#include "DDImage/Row.h"
#include "DDImage/DeepPixelOp.h"
#include "DDImage/DeepFilterOp.h"

using namespace DD::Image;

class DeepRemove : public DeepFilterOp
{
  ChannelSet channels;
  ChannelSet channels2;
  ChannelSet channels3;
  ChannelSet channels4;

  int operation; // 0 = remove, 1 = keep

public:
  void _validate(bool);
  DeepRemove(Node* node) : DeepFilterOp(node)
  {
    channels = Mask_All;
    channels2 = channels3 = channels4 = Mask_None;
    operation = 0;
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

  /*virtual*/
    void getDeepRequests(Box bbox, const DD::Image::ChannelSet& channels, int count, std::vector<RequestData>& requests) {
        if (!input0())
            return;
        DD::Image::ChannelSet get_channels = channels;
        requests.push_back(RequestData(input0(), bbox, get_channels, count));
    }

  virtual bool doDeepEngine(Box box, const ChannelSet& channels, DeepOutputPlane& outPlane)
  {
    // DeepOp* in = input0();
    DeepPlane inPlane;
    outPlane = DeepOutputPlane(channels, box);
    ChannelSet get_channels = channels;
    if (!input0()->deepEngine(box, get_channels, inPlane)){
        // std::cout << "inplane could not be set" << std::endl;
        return false;
    }
    
    const int nOutputChans = get_channels.size();

    for (Box::iterator it = box.begin(); it != box.end(); it++) {
        if (Op::aborted()) 
        {
            return false; 
        }
        
        DeepPixel in_pixel = inPlane.getPixel(it);
        const unsigned nSamples = in_pixel.getSampleCount();

        DeepOutPixel out_pixel;
        out_pixel.reserve(nSamples * nOutputChans);

        for (unsigned i = 0; i < nSamples; ++i) {
            foreach(z, get_channels) {
                out_pixel.push_back(in_pixel.getUnorderedSample(i, z));
            }
        }
        outPlane.addPixel(out_pixel);

    }
    return true;
  }

  virtual void knobs(Knob_Callback);
  const char* Class() const { return RCLASS; }
  const char* node_help() const { return HELP; }
  static Iop::Description d;
};

void DeepRemove::_validate(bool for_real)
{
  // NOTE: MUST ALWAYS KEEP THE DEEP CHANNEL - OTHERWISE NODE CANNOT OPERATE!
  
  DeepFilterOp::_validate(for_real);
  if (input0()) {
      input0()->validate(true);
      DeepInfo srcDeepInfo = input0()->deepInfo();

      ChannelSet d = Mask_Deep; // keep the deep

      ChannelSet c = channels;
      c += (channels2);
      c += (channels3);
      c += (channels4);
      if (operation) {
        ChannelSet new_chan = srcDeepInfo.channels();
        c += d;
        new_chan &= (c);
        _deepInfo = DeepInfo(srcDeepInfo.formats(), srcDeepInfo.box(), new_chan);
      }
      else {
        ChannelSet new_chan = srcDeepInfo.channels();
        new_chan -= (c);
        _deepInfo = DeepInfo(srcDeepInfo.formats(), srcDeepInfo.box(), new_chan);
      }

  } else {
    // need to do some handling here
    // std::cout << "validate failed for some reason" << std::endl;
  }
}

static const char* const enums[] = {
  "remove", "keep", 0
};

void DeepRemove::knobs(Knob_Callback f)
{
  Enumeration_knob(f, &operation, enums, "operation");
  Tooltip(f, "Remove: the named channels are deleted\n"
             "Keep: all but the named channels are deleted");
  Obsolete_knob(f, "action", "knob operation $value");
  Input_ChannelMask_knob(f, &channels, 0, "channels");
  Input_ChannelMask_knob(f, &channels2, 0, "channels2", "and");
  Input_ChannelMask_knob(f, &channels3, 0, "channels3", "and");
  Input_ChannelMask_knob(f, &channels4, 0, "channels4", "and");
}

static Op* build(Node* node) { return new DeepRemove(node); }
Op::Description DeepRemove::d(RCLASS, "Color/DeepRemove", build);