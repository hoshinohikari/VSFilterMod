## 20260225
1. 在CMyLua中引入了一种启用/禁用Lua状态的机制，从而实现了更优的错误处理，并支持回退到非Lua渲染。
2. 添加了用于检查是否已启用Lua以及出于特定原因禁用Lua的方法。
3. 改进了Lua脚本加载和执行时的错误报告功能。
4. 更新了VobSubFile以正确处理64位PTS值，并重构了调色板处理以提高清晰度。
5. 在DirectVobSub中引入了新的设置，用于控制字幕动画和缓冲行为，包括设置安全钳制值。
6. 增强了DirectVobSubFilter的功能，使其能够根据新设置初始化字幕队列。
7. 为IDirectVobSub接口中的新属性添加了支持，以便更好地控制字幕渲染。
8. 为提高代码的清晰度和可维护性，对代码进行了重构，包括在VobSubFile中使用内联函数进行标记转换。

## 20260123
1. 新增接口 `IDirectVobSub3`
2. `RenderingCache` 的迁移与接入，增加文本宽度缓存 与 矢量多边形解析缓存
3. 增加 `SubtitleInputPin` 的异步解码机制
4. 增加对 `WebVTT` 的支持
5. 完整移植 MPC‑HC 的 `ColorConvTable` 逻辑（BT.601/709/2020 + TV/PC Range 处理）
6. 引入 `SubtitleHelpers / SubType`，统一字幕类型
7. 引入 `StdioFile64`，支持超大字幕文件（>2GB）读写