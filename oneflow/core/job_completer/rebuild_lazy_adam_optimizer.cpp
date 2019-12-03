#include "oneflow/core/job_completer/rebuild_lazy_adam_optimizer.h"
#include "oneflow/core/job/job_desc.h"
#include "oneflow/core/common/protobuf.h"

namespace oneflow {

bool IsOpConstantOne(const OperatorConf& op_conf) {
  if (!op_conf.has_constant_conf()) { return false; }
  const InitializerConf& initializer_conf = op_conf.constant_conf().initializer();
  if (initializer_conf.has_constant_int_conf()) {
    return initializer_conf.constant_int_conf().value() == 1;
  } else if (initializer_conf.has_constant_conf()) {
    return initializer_conf.constant_conf().value() == 1;
  } else {
    UNIMPLEMENTED();
    return false;
  }
}

void RebuildLazyAdamOptimizer(const OpGraph& op_graph, JobBuilder* job_builder) {
  std::vector<OperatorConf> lazy_adam_optimizers;
  op_graph.ForEachNode([&](OpNode* op_node) {
    const OperatorConf& op_conf = op_node->op().op_conf();
    if (op_conf.has_lazy_adam_model_update_conf()) {
      lazy_adam_optimizers.push_back(op_node->op().op_conf());
    }
  });
  FOR_RANGE(int64_t, i, 0, lazy_adam_optimizers.size()) {
    OperatorConf* op_conf = &lazy_adam_optimizers.at(i);
    const std::string& total_instance_num_diff_lbn =
        op_conf->lazy_adam_model_update_conf().total_instance_num_diff();
    if (total_instance_num_diff_lbn.empty()) { continue; }
    const OperatorConf& producer_op_conf =
        op_graph.OpNode4OpName(GenLogicalBlobId(total_instance_num_diff_lbn).op_name())
            ->op()
            .op_conf();
    if (!IsOpConstantOne(producer_op_conf)) { continue; }
    op_conf->mutable_lazy_adam_model_update_conf()->clear_total_instance_num_diff();
  }

  using Key = std::pair<std::pair<std::string, std::string>, std::pair<float, float>>;
  HashMap<Key, std::vector<OperatorConf*>> key2ops;
  FOR_RANGE(int64_t, i, 0, lazy_adam_optimizers.size()) {
    OperatorConf* op_conf = &lazy_adam_optimizers.at(i);
    const LazyAdamModelUpdateOpConf& lazy_adam_model_update_conf =
        op_conf->lazy_adam_model_update_conf();
    Key key = std::make_pair(
        std::make_pair(lazy_adam_model_update_conf.learning_rate(),
                       lazy_adam_model_update_conf.train_step()),
        std::make_pair(lazy_adam_model_update_conf.user_conf().lazy_adam_conf().beta1(),
                       lazy_adam_model_update_conf.user_conf().lazy_adam_conf().beta2()));
    key2ops[key].push_back(op_conf);
  }
  job_builder->MutOpsOnlyOnce(lazy_adam_optimizers);
}

}  // namespace oneflow
