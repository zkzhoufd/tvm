/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file quantize.cc
 *
 * \brief transform a graph to a low-bit graph
 *   for compression and acceleration.
 */
#include "./quantize.h"

#include <dmlc/thread_local.h>
#include <tvm/relay/op_attr_types.h>
#include <tvm/relay/transform.h>

#include <stack>

namespace tvm {
namespace relay {
namespace quantize {

TVM_REGISTER_NODE_TYPE(SimulatedQuantizeAttrs);

bool SimulatedQuantizeRel(const Array<Type>& types, int num_inputs, const Attrs& attrs,
                          const TypeReporter& reporter) {
  ICHECK_EQ(types.size(), 6);
  const auto param = attrs.as<SimulatedQuantizeAttrs>();
  ICHECK(param != nullptr);

  const auto* data = types[0].as<TensorTypeNode>();

  if (data == nullptr) {
    return false;
  }
  //printf("%s\n", param->name.c_str());
  //ICHECK_NE(data->shape.size(), 0) << "Input shape cannot be empty";

  if(param->kind == QAnnotateKind::kQWeight && param->per_channel){
    // if(data->shape.size() == 0){
    //   reporter->Assign(types[1], TensorType({1}, DataType::Float(32)));  // dom_scale
    // }
    // else{
    //   reporter->Assign(types[1], TensorType({data->shape[0], 1, 1, 1}, DataType::Float(32)));  // dom_scale

    // }
    reporter->Assign(types[1], TensorType({data->shape[0], 1, 1, 1}, DataType::Float(32)));  // dom_scale
    reporter->Assign(types[2], TensorType({}, DataType::Float(32)));  // clip_min
    reporter->Assign(types[3], TensorType({}, DataType::Float(32)));  // clip_max
    if(param->asymmetric){
      reporter->Assign(types[4], TensorType({data->shape[0], 1, 1, 1}, DataType::Float(32)));  // zero_point
    }
    else{
      reporter->Assign(types[4], TensorType({}, DataType::Float(32)));
    }
    reporter->Assign(types[5], types[0]);                             // output
  } else {
    reporter->Assign(types[1], TensorType({1}, DataType::Float(32)));  // dom_scale
    reporter->Assign(types[2], TensorType({}, DataType::Float(32)));  // clip_min
    reporter->Assign(types[3], TensorType({}, DataType::Float(32)));  // clip_max
    if(param->asymmetric)
      reporter->Assign(types[4], TensorType({1}, DataType::Float(32)));  // zero_point
    else
      reporter->Assign(types[4], TensorType({}, DataType::Float(32)));
    reporter->Assign(types[5], types[0]);                             // output
  }
  return true;
}

RELAY_REGISTER_OP("relay.op.annotation.simulated_quantize")
    .describe(R"code(simulated quantize op)code" TVM_ADD_FILELINE)
    .set_num_inputs(5)
    .add_argument("data", "Tensor", "The input data.")
    .add_argument("dom_scale", "Tensor", "The domain scale of input data. It should be a scalar or vector")
    .add_argument("clip_min", "Tensor", "lower bound. It should be a scalar")
    .add_argument("clip_max", "Tensor", "upper bound. It should be a scalar")
    .add_argument("zero_point", "Tensor", "Zero point for asymmetric quantization")
    .set_attrs_type<SimulatedQuantizeAttrs>()
    .set_support_level(11)
    .add_type_rel("SimulatedQuantize", SimulatedQuantizeRel);

TVM_REGISTER_GLOBAL("relay._quantize.simulated_quantize")
    .set_body_typed([](Expr data, Expr dom_scale, Expr clip_min, Expr clip_max, Expr zero_point, int kind,
                       String rounding, bool per_channel, bool asymmetric, String name) {
      auto attrs = make_object<SimulatedQuantizeAttrs>();
      attrs->kind = kind;
      attrs->rounding = rounding;
      attrs->per_channel = per_channel;
      attrs->asymmetric = asymmetric;
      attrs->name = name;
      static const Op& op = Op::Get("relay.op.annotation.simulated_quantize");
      return Call(op, {data, dom_scale, clip_min, clip_max, zero_point}, Attrs(attrs), {});
    });

/*! \brief Entry to hold the BuildConfig context stack. */
struct TVMQConfigThreadLocalEntry {
  /*! \brief The default build config if the stack is empty */
  QConfig default_config;

  /*! \brief The current build config context */
  std::stack<QConfig> context_stack;

  TVMQConfigThreadLocalEntry() : default_config(make_object<QConfigNode>()) {}
};

/*! \brief Thread local store to hold the BuildConfig context stack. */
typedef dmlc::ThreadLocalStore<TVMQConfigThreadLocalEntry> TVMQConfigThreadLocalStore;

void QConfig::EnterQConfigScope(const QConfig& build_config) {
  TVMQConfigThreadLocalEntry* entry = TVMQConfigThreadLocalStore::Get();
  entry->context_stack.push(build_config);
}

void QConfig::ExitQConfigScope() {
  TVMQConfigThreadLocalEntry* entry = TVMQConfigThreadLocalStore::Get();
  entry->context_stack.pop();
}

QConfig& QConfig::Current() {
  TVMQConfigThreadLocalEntry* entry = TVMQConfigThreadLocalStore::Get();
  if (entry->context_stack.size() > 0) {
    return entry->context_stack.top();
  }

  return entry->default_config;
}

TVM_REGISTER_NODE_TYPE(QConfigNode);

TVM_STATIC_IR_FUNCTOR(ReprPrinter, vtable)
    .set_dispatch<QConfigNode>([](const ObjectRef& ref, ReprPrinter* p) {
      auto* op = static_cast<const QConfigNode*>(ref.get());
      p->stream << "qconfig(";
      p->stream << "nbit_input=" << op->nbit_input << ", ";
      p->stream << "nbit_weight=" << op->nbit_weight << ", ";
      p->stream << "nbit_activation=" << op->nbit_activation << ", ";
      p->stream << "estimator_activation=" << op->estimator_activation << ", ";
      p->stream << "global_scale=" << op->global_scale << ", ";
      p->stream << "estimator_weight=" << op->estimator_weight << ", ";
      p->stream << "skip_conv_layers==" << op->skip_conv_layers << ", ";
      p->stream << "skip_dense_layer==" << op->skip_dense_layer << ", ";
      p->stream << "do_simulation==" << op->do_simulation << ", ";
      p->stream << "round_for_shift==" << op->round_for_shift << ", ";
      p->stream << "debug_enabled_ops==" << op->debug_enabled_ops << ", ";
      p->stream << "rounding==" << op->rounding << ", ";
      p->stream << "partition_conversions==" << op->partition_conversions;
      p->stream << ")";
    });

TVM_REGISTER_GLOBAL("relay._quantize._GetCurrentQConfig").set_body_typed([]() -> QConfig {
  return QConfig::Current();
});

TVM_REGISTER_GLOBAL("relay._quantize._EnterQConfigScope")
    .set_body_typed(QConfig::EnterQConfigScope);

TVM_REGISTER_GLOBAL("relay._quantize._ExitQConfigScope").set_body_typed(QConfig::ExitQConfigScope);

}  // namespace quantize
}  // namespace relay
}  // namespace tvm
