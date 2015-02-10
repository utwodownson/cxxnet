#ifndef PRELU_LAYER_INL_HPP
#define PRELU_LAYER_INL_HPP
#pragma once

#include <mshadow/tensor.h>
#include "./layer.h"
#include "./op.h"


namespace cxxnet {
namespace layer {

template<typename xpu>
class PReluLayer : public ILayer<xpu> {
 public:
  PReluLayer(mshadow::Random<xpu> *p_rnd) : prnd_(p_rnd) {
    // setup default value
    init_slope_ = 3.0;
  }
  virtual void SetParam(const char *name, const char* val) {
    if (!strcmp(name, "init_slope")) init_slope_ = atof(val);
  }
  virtual void ApplyVisitor(typename ILayer<xpu>::IVisitor *pvisitor) {
    pvisitor->Visit("wmat", slope_, gslope_);
  } 
  virtual void InitConnection(const std::vector<Node<xpu>*> &nodes_in,
                              const std::vector<Node<xpu>*> &nodes_out,
                              ConnectState<xpu> *p_cstate) {
    utils::Check(nodes_in.size() == 1 && nodes_out.size() == 1,
                 "PReluLayer: only support 1-1 connection");
    
    nodes_out[0]->data.shape_ = nodes_in[0]->data.shape_;
    if (nodes_in[0]->data.size(1) == 1){
      // This is a fc layer
      channel_ = nodes_in[0]->data.size(3);
    } else {
      // This is a conv layer
      channel_ = nodes_in[0]->data.size(1);
    }
  }
  virtual void InitModel(void) {
    // resize to correct shape
    slope_.Resize(mshadow::Shape1(channel_));
    gslope_.Resize(mshadow::Shape1(channel_));
    slope_ = init_slope_;
    gslope_ = 0.0;
  }
  virtual void SaveModel(utils::IStream &fo) const{
    slope_.SaveBinary(fo);
  }
  virtual void LoadModel(utils::IStream &fi){  
    slope_.LoadBinary(fi);
    // setup gradient weight
    gslope_.Resize(slope_.shape_);
    gslope_ = 0.0f;
  }
  virtual void SetStream(mshadow::Stream<xpu> *stream) {
    slope_.set_stream(stream);
    gslope_.set_stream(stream);
  }
  virtual void OnBatchSizeChanged(const std::vector<Node<xpu>*> &nodes_in,
                                  const std::vector<Node<xpu>*> &nodes_out,
                                  ConnectState<xpu> *p_cstate) {
    // Do nothing for now
  }
  virtual void Forward(bool is_train,
                       const std::vector<Node<xpu>*> &nodes_in,
                       const std::vector<Node<xpu>*> &nodes_out,
                       ConnectState<xpu> *p_cstate) {
    using namespace mshadow::expr;
    mshadow::Tensor<xpu, 4> &in = nodes_in[0]->data;
    mshadow::Tensor<xpu, 4> &out = nodes_out[0]->data;
    if (in.size(1) != 1){
      in = F<op::xelu>(in, broadcast<1>(slope_, in.shape_));
    } else {
      in = F<op::xelu>(in, broadcast<3>(slope_, in.shape_));
    }
    mshadow::Copy(out, in, out.stream_);
  }
  virtual void Backprop(bool prop_grad,
                        const std::vector<Node<xpu>*> &nodes_in,
                        const std::vector<Node<xpu>*> &nodes_out,
                        ConnectState<xpu> *p_cstate) {
    using namespace mshadow::expr;
    mshadow::Tensor<xpu, 4> &in = nodes_in[0]->data;
    mshadow::Tensor<xpu, 4> &out = nodes_out[0]->data;
    if (in.size(1) != 1){
      gslope_ += sumall_except_dim<1>(out);
      if (prop_grad){
        in = F<op::xelu_grad>(in, broadcast<1>(slope_, in.shape_)) * out;
      }
    } else {
      gslope_ += sumall_except_dim<3>(out);
      if (prop_grad){
        in = F<op::xelu_grad>(in, broadcast<3>(slope_, in.shape_)) * out;
      }
    }
  }

 private:
  /*! \brief random number generator */
  mshadow::Random<xpu> *prnd_;
  /*! \brief init slope */
  float init_slope_;
  /*! \brief the number of channels */
  int channel_;
  /*! \brief slope */
  mshadow::TensorContainer<xpu,1> slope_;
  /*! \brief gradient of slope */
  mshadow::TensorContainer<xpu,1> gslope_;
  
};  // class PReluLayer

} // namespace layer
} // namespace cxxnet
#endif // INSANITY_LAYER_INL_HPP
