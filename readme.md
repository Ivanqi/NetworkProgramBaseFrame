# NetWorker
## 程序安装
- 执行根目录下的 build.sh脚本, 对应生成的文件会存放在 build目录下
  - ./build.sh 类似于 make操作
  - ./build.sh 类似于 make install 操作
- 把build目录中生成的头文件和静态链接库移动到系统目录下(Linux)
  - 默认的系统include目录: /usr/include
  - 默认的系统lib目录: /usr/local/lib
  - ./migration.sh