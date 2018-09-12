#include "DDImage/DeepPixelOp.h"
// #include "DDImage/DeepOp.h"
#include "DDImage/Knobs.h"
#include "DDImage/RGB.h"

static const char* CLASS = "DeepPlus";

using namespace DD::Image;

class DeepPlus : public DeepPixelOp
{
public:
  DeepPlus(Node* node) : DeepPixelOp(node) {
  }

  const char* node_help() const
  {
    return "Merge two Deep channels as a plus operation";
  }

  const char* Class() const {
    return CLASS;
  }

  virtual void in_channels(int, ChannelSet& channels) const
  {
    if (channels & Mask_RGB)
      channels += Mask_RGB;
  }

  virtual void processSample(int y, int x, const DD::Image::DeepPixel& deepPixel, size_t sampleNo, const DD::Image::ChannelSet& channels, DeepOutPixel& output) const;
};


void DeepPlus::processSample(int y, int x, const DD::Image::DeepPixel& deepPixel, size_t sampleNo, const DD::Image::ChannelSet& channels, DeepOutPixel& output) const
  {
    bool madeLuma = false;
    float luma;
    foreach(z, channels) {
      if (z == Chan_Red || z == Chan_Green || z==Chan_Blue) {
        if (!madeLuma) {
          luma = y_convert_rec709(deepPixel.getUnorderedSample(sampleNo, Chan_Red),
                                  deepPixel.getUnorderedSample(sampleNo, Chan_Green),
                                  deepPixel.getUnorderedSample(sampleNo, Chan_Blue));
          madeLuma = true;
        }

        output.push_back(luma);
      } else {
        output.push_back(deepPixel.getUnorderedSample(sampleNo, z));
      }
    }
  }

static Op* build(Node* node) { return new DeepPlus(node); }
static const Op::Description d(CLASS, "Image/DeepPlus", build);
