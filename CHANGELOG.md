## 20260123
1. 新增接口 `IDirectVobSub3`
2. `RenderingCache` 的迁移与接入，增加文本宽度缓存 与 矢量多边形解析缓存
3. 增加 `SubtitleInputPin` 的异步解码机制
4. 增加对 `WebVTT` 的支持
5. 完整移植 MPC‑HC 的 `ColorConvTable` 逻辑（BT.601/709/2020 + TV/PC Range 处理）
6. 引入 `SubtitleHelpers / SubType`，统一字幕类型
7. 引入 `StdioFile64`，支持超大字幕文件（>2GB）读写