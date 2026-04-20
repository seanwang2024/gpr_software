# 如何让 Claude Code 生成第一个有窗体的 Qt 桌面应用

本文档指导 Claude Code 自动生成一个基于 Qt 6.8.3 的最小桌面应用程序。

## 要求
- 使用 CMake 构建系统
- 包含一个主窗口，有一个QLable和一个按钮
- 点击按钮时，弹出一个窗体，选择文件
- 选定文件目录和文件名， 在QLable显示输入的图片文件
- 项目文件结构清晰（src/ 和 include/ 分离）

## 生成步骤

1. **创建项目根目录**，例如 `my_qt_app/`

2. **编写 CMakeLists.txt**（内容见上）
   - 设置 CMake 最低版本 3.16
   - 启用 AUTOMOC（处理 Qt 元对象）
   - 查找 Qt6::Core 和 Qt6::Widgets
   - 添加可执行文件并链接 Qt 库

3. **创建头文件和源文件**
   - `include/MainWindow.h`：声明 MainWindow 类，继承 QMainWindow
   - `src/MainWindow.cpp`：实现构造函数，创建布局、标签和按钮，连接信号槽
   - `src/main.cpp`：QApplication 入口，创建并显示 MainWindow

4. **构建和运行**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64/lib/cmake"
   cmake --build .
   windeployqt MyQtApp.exe
   ./MyQtApp

5. **git push**
   ## Git 自动化规则
  完成任意任务后，必须执行以下命令：
   `git add -A`
   `git commit -m "[简短描述本次更改]"`
   `git push origin [当前分支名]`


