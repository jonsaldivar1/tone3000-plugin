#include "NamSlot.h"

#include <cmath>
#include <filesystem>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/slimmable.h"

namespace parabola {

NamSlot::NamSlot() = default;
NamSlot::~NamSlot() = default;

NamSlot::Info NamSlot::readInfo(const juce::File& file) {
  Info info;
  info.path = file.getFullPathName();
  info.name = file.getFileNameWithoutExtension();
  const auto json = juce::JSON::parse(file);
  if (auto* obj = json.getDynamicObject()) {
    const auto meta = obj->getProperty("metadata");
    if (meta.isObject()) {
      if (const auto name = meta["name"].toString(); name.isNotEmpty()) info.name = name;
      info.gearType = meta["gear_type"].toString();
      info.modeledBy = meta["modeled_by"].toString();
      if (meta.hasProperty("loudness") && !meta["loudness"].isVoid()) {
        const double l = meta["loudness"];
        if (std::isfinite(l) && l > -80.0 && l < 20.0) {
          info.hasLoudness = true;
          info.loudnessDb = l;
        }
      }
    }
    const auto rate = obj->getProperty("sample_rate");
    if (!rate.isVoid() && static_cast<double>(rate) > 0) info.expectedRate = rate;
  }
  const auto gear = info.gearType.toLowerCase();
  info.fullRig = gear.contains("full") || gear.contains("cab");
  return info;
}

juce::String NamSlot::load(const juce::File& file) {
  if (!file.existsAsFile()) return "File not found: " + file.getFullPathName();
  std::unique_ptr<nam::DSP> model;
  try {
#if JUCE_WINDOWS
    const std::filesystem::path path(file.getFullPathName().toWideCharPointer());
#else
    const std::filesystem::path path(file.getFullPathName().toStdString());
#endif
    model = nam::get_dsp(path);
  } catch (const std::exception& e) {
    return "Couldn't load " + file.getFileName() + ": " + juce::String(e.what());
  }
  if (model == nullptr) return "Couldn't load " + file.getFileName();
  if (auto* slim = dynamic_cast<nam::SlimmableModel*>(model.get())) slim->SetSlimmableSize(1.0);
  model->ResetAndPrewarm(sampleRate_, maxBlock_);

  auto info = readInfo(file);
  const float gain = info.hasLoudness ? std::pow(10.0f, (kTargetLoudnessDb - static_cast<float>(info.loudnessDb)) / 20.0f) : 1.0f;
  {
    const juce::SpinLock::ScopedLockType lock(lock_);
    std::swap(model_, model);
    normGain_ = gain;
  }
  model.reset();  // the old one, off the audio thread
  fullRig_.store(info.fullRig);
  {
    const juce::ScopedLock l(infoLock_);
    info_ = std::move(info);
  }
  loaded_.store(true);
  return {};
}

void NamSlot::clear() {
  std::unique_ptr<nam::DSP> old;
  {
    const juce::SpinLock::ScopedLockType lock(lock_);
    std::swap(model_, old);
  }
  old.reset();
  loaded_.store(false);
  fullRig_.store(false);
  const juce::ScopedLock l(infoLock_);
  info_ = {};
}

void NamSlot::prepare(double sampleRate, int maxBlockSize) {
  sampleRate_ = sampleRate;
  maxBlock_ = juce::jmax(16, maxBlockSize);
  in_.assign(static_cast<size_t>(maxBlock_), 0.0);
  out_.assign(static_cast<size_t>(maxBlock_), 0.0);
  const juce::SpinLock::ScopedLockType lock(lock_);
  if (model_ != nullptr) model_->ResetAndPrewarm(sampleRate_, maxBlock_);
}

NamSlot::Info NamSlot::info() const {
  const juce::ScopedLock l(infoLock_);
  return info_;
}

void NamSlot::process(float* io, int n) {
  const juce::SpinLock::ScopedTryLockType lock(lock_);
  if (!lock.isLocked() || model_ == nullptr) return;
  for (int start = 0; start < n; start += maxBlock_) {
    const int frames = juce::jmin(maxBlock_, n - start);
    for (int i = 0; i < frames; ++i) in_[static_cast<size_t>(i)] = io[start + i];
    double* inPtr = in_.data();
    double* outPtr = out_.data();
    model_->process(&inPtr, &outPtr, frames);
    for (int i = 0; i < frames; ++i) {
      const auto y = static_cast<float>(out_[static_cast<size_t>(i)]) * normGain_;
      io[start + i] = std::isfinite(y) ? y : 0.0f;
    }
  }
}

}  // namespace parabola
