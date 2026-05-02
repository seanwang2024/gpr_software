# 如何让 Claude Code 生成第一个有窗体的 Qt 桌面应用

本文档指导 Claude Code 自动生成一个基于 Qt 6.8.3 的最小桌面应用程序。

## 要求
- 使用 CMake 构建系统
- 包含一个主窗口，有一个QLable和一个按钮
- 点击按钮时，弹出一个窗体，选择文件
- 选定文件目录和文件名， 在QLable显示输入的自定义图片文件，文件格式详见specs/dzt_file.md
- Qlable显示不需要缩放，图片多大就显示多大
- 项目文件结构清晰（src/ 和 include/ 分离）

## 2026年5月2日功能(增量开发) ribbon风格 顶部是一级标签栏，下面是横向排列的图标+文字按钮组，每组底部有分组名。
1. 生成菜单menu包含原来的Qlabel和左边的空间，一级菜单为文字 二级菜单包含图标和文字(分组名)
2. Menu一级内容为 开始、数据处理
3. 开始下面二级菜单 文件设置(打开 保存 关闭 ) 图像缩放( 水平放大 水平缩小 图像还原 堆积图 调色板) 简易处理(一键处理 调节零点 校正零偏 背景消除 调节增益 数字滤波 批处理)
4. 二级菜单各项图标ICON在 resoures/ 文件名为二级菜单分项名字 比如 打开.png

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


