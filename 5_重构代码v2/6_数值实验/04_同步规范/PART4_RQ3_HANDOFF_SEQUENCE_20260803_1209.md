# Part 4 / RQ3后续交接顺序

当前允许执行screening，不允许启动Part 4正式矩阵。

1. 确认A3和RQ2均为`FINAL_COMPLETE_AND_FROZEN`；
2. 确认正式Tag仍解析到冻结算法提交；
3. 确认`src/`和`include/`未变化；
4. 将本矩阵与设计说明提交至GitHub；
5. 建立42次可续跑screening队列；
6. 先执行seed 101的3个预检情景；
7. 预检通过后继续剩余39次任务；
8. 生成碳价汇总、配额汇总、恒等式审计和与RQ2基准的一致性检查；
9. 推荐7个正式碳价水平；
10. 将Part4 screening标记为`FINAL_COMPLETE_AND_FROZEN`；
11. 将Part4 formal保持为`PENDING_USER_REVIEW`；
12. 停止并等待用户确认。

Screening结束前不得启动Part4正式实验；任何阶段不得启动Part5，不得修改`src/`、`include/`、A3或RQ2。
