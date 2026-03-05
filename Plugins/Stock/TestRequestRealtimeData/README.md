# RequestRealtimeData 测试 Demo

用于测试股票插件中 `RequestRealtimeData` 的请求逻辑，运行后会输出**调用的参数**和**接口信息**。

## 运行方式

### 方式一：Cursor / VS Code

1. 确保已安装 **Microsoft C/C++** 扩展（调试需要）。
2. 若未用过 MSBuild，请先从开始菜单打开 **“Developer Command Prompt for VS 2022”**（或 “x64 Native Tools Command Prompt”），在该终端里执行 `code .` 或 `cursor .` 打开当前项目，这样终端里会有 `msbuild`。
3. 在 Cursor 中按 **F5** 或点击 **运行 → 启动调试**，在顶部下拉里选择 **“运行 TestRequestRealtimeData”** 再运行。
4. 会先自动编译，再在集成终端里运行测试程序并看到输出。

### 方式二：Visual Studio

1. 打开 `TrafficMonitorPlugins.sln`
2. 将 **TestRequestRealtimeData** 设为启动项目（右键 → 设为启动项目）
3. 选择配置（如 Debug | x64）后按 F5 运行

### 方式三：Python（推荐，无需编译）

仅需本机已安装 Python 3，无需任何第三方库：

```bash
cd Plugins/Stock/TestRequestRealtimeData
python test_request_realtime_data.py
```

自定义股票代码（空格分隔）：

```bash
python test_request_realtime_data.py sz000001 sh600000
```

### 方式四：命令行编译（C++）

在 **VS 开发者命令提示符** 中进入本目录后执行：

```bat
cl /EHsc /std:c++17 /Fe:TestRealtime.exe TestRequestRealtimeData.cpp winhttp.lib
TestRealtime.exe
```

## 自定义股票代码

- **默认**：`sz000759`、`sh600519`
- **命令行传参**：  
  `TestRealtime.exe sz000001 sh600000`  
  多个代码用空格分隔，与插件格式一致（sh 沪市 / sz 深市 / hk 港股等）。

## 输出说明

程序会打印：

| 项       | 说明 |
|----------|------|
| 接口名称 | 新浪股票实时行情 |
| 接口地址 | `https://hq.sinajs.cn/?` |
| 请求方式 | GET |
| 调用参数 | `_`（防缓存随机数）、`list`（股票代码，逗号分隔） |
| 完整 URL | 实际请求的完整地址 |
| 请求头   | User-Agent、Referer |
| 请求结果 | 是否成功、响应长度、响应内容预览 |

响应内容为新浪返回的 GBK 编码，控制台为 UTF-8 时中文可能显示为乱码，仅作“是否返回数据”和长度参考即可。
