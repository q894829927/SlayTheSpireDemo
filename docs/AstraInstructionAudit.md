# GPT-6 Astra 指令精简记录

日期：2026-09-09。本文件记录本次修改依据与范围，不是新增的常驻指令或开发阶段。

## 阅读依据

- [OpenAI 官方 Model Guidance](https://developers.openai.com/api/docs/guides/latest-model)：通过官方 `.md` 页面完整读取 Astra 正文，包括行为、提示建议和迁移部分。采用其按改动校准测试、明确授权边界与完成范围的建议；未修改模型、推理等级或 API 配置。
- [Eric Provencher 原文](https://x.com/pvncher/status/2095991462416490862)，2026-09-04，Rethinking skills and prompts for GPT-6 Astra：X 直连返回 403；通过[公开镜像](https://x.noodl3.net/i/article/2095991462416490862)完整读取英文正文。采用按需披露文档、去除过时流程脚手架、保留有实际用途的边界等经验。镜像不是官方托管页。

这些建议支持减少重复流程，不代表可以删除项目验收要求或假定模型永不出错。未照搬官方可选的主动委派提示；当前会话不允许无明确要求的主动委派。

## 实际指令入口

完整检查项目内 8 份 AGENTS.md：根目录、Source、Runtime、Cards、Presentation、UI、Tests、Content。子目录规则仅适用于其子树。

项目目录（包括隐藏/忽略的非生成目录）未发现 SKILL.md、AGENTS.override.md、CLAUDE.md、GEMINI.md、Cursor .mdc 或 Copilot instructions；未发现项目 .codex/.agents 指令目录。父目录未发现 AGENTS/override；用户级 `C:/Users/zhang/.codex/AGENTS.md` 为空。全局 config.toml 的指令覆盖/项目文档/agent/skill 配置键定向检查未发现额外入口。

本次按需加载并完整读取的技能是用户安装环境提供的 `openai-docs/SKILL.md` 及其 model-migration 参考。其“仅官方域”和“仅读相关章节”默认范围不覆盖用户明确要求的 Eric 原文及全文阅读；本次遵循用户要求扩展阅读。没有修改内置技能缓存。其他技能仅在目录中可用，并不表示其正文全部对本项目生效；未为本次 UE 指令审计加载宠物、文档渲染、网站等无关工作流。

应用注入的工具、安全、委派和技能规则仍约束本会话，仓库文件不能改写它们。已经注入当前会话的旧根指令也不会因磁盘修改自动被撤回。

其他持久化材料按职责检查：ValidationExecutionPolicy 是执行政策；Architecture、DevelopmentPhases、LegacyUIPreservationPolicy 及相关 Selection 设计用于核对约束和状态；CODEX_GOAL_CHECKPOINT 是恢复线索。未重审已封存阶段的详细测试证据。GitHub workflow 是显式触发的执行配置，不是每次编辑都要运行的指令，本次未修改。

## 修改与保留

| 范围 | 修改及原因 |
|---|---|
| 根 AGENTS | 文档入口改为按受影响契约使用；合并重复验证和时间线表述；明确在已授权范围内自主完成编辑、验证、文档。保留完整 UE 5.8 项目生成和构建命令。 |
| Source / Tests | 去重模块、编辑与验证流程；已有测试足以覆盖契约时不强制再写测试。保留 Editor-only、Shipping 隔离、GC、确定性与真实证据要求。 |
| Runtime | 删除“遗物模型尚待 Phase 7”和“遗物到来即抽取多源 modifier”的过时表述。Architecture 与现有 RelicContainer/Instance 已定义所有权；Damage/Block 管线仍可直接收集 Status，遗物 Trigger 的存在不要求额外框架。 |
| UI / Presentation | 六文档/全目录预读改为契约路由；纠正 A2E/A3 当前阶段标签；明确 Selection/group 规则是设计目标，不能据此假称已经实现或启动计划阶段。保留 token、历史快照、ownership、watermark、group timeout 等具体边界。 |
| Content | 移除旧 A2E remaining-steps 当前入口；人工 UE 步骤按具体编辑提供，避免每个资产任务都输出完整节点清单。 |
| ValidationExecutionPolicy | 将重复预算、停止、截图、失败、委派段落合并；明确纯文档无需构建，PASS 复用，失败只重跑失效 Gates。移除固定角色示例和旧自动 commit/STOP 提示模板；仍须完成所有已授权工作并遵守人工 Gate。保留 A2N 表并标明历史范围，防止其 Legacy parity 被当作新测试授权。 |
| Cards AGENTS | 原样保留：中文本地化、Effect 自动描述、参数隔离和预览契约是具体内容验收，且已有按影响验证规则。 |

保留的安全/项目边界：Gameplay 唯一权威、Action/Queue 顺序、确定性、定义不可变、事件/Trigger 只读、Gameplay 不等待 Presentation、Native-only、Legacy 资产保护及单独授权、新依赖/插件/引擎/构建设置授权、阶段依赖、用户改动保护、不提交生成物、不虚报验证。

## 验证与限制

本次只修改 Markdown 指令和政策；检查差异、引用路径、旧状态表述和 `git diff --check`。没有执行 UE 构建、Automation、PIE，没有修改资产、代码、插件或全局配置，也没有创建提交。未改变现有开发阶段或人工验收状态。

精简减少的是文件文本和重复规则；未进行模型运行 A/B 测试，不能将行数减少直接等同于实际 Token 或费用节省。
