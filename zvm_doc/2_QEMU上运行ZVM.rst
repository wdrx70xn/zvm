在ARM QEMU上运行ZVM
======================


1. QEMU 平台构建ZVM镜像
-----------------------

拉取镜像并进入工作区：

.. code:: shell

   cd ~/zvm_workspace/zvm

1） 使用脚本文件构建ZVM镜像：

.. code:: shell

   ./auto_zvm.sh build qemu_max_smp

如果遇到：

.. code:: shell

    Could not find a package configuration file provided by "Zephyr" with any ...

请手动指定Cmake的路径：

.. code:: shell

   export CMAKE_PREFIX_PATH=$(pwd)

此外，还有可能遇到elftool等包未安装的问题，可以直接用：

.. code:: shell

   pip install xxx

等命令安装即可。

除了使用上述脚本外，也可以使用命令行构建镜像:

.. code:: shell

   west build -b qemu_max_smp samples/subsys/zvm


2） 生成ZVM镜像文件如下:

.. code:: shell

    build/zephyr/zvm_host.elf


2. QEMU 平台运行ZVM(非定制镜像)
-------------------------------

如果不想自己去定制Linux和Zephyr的镜像文件，本项目提供了直接可以在平台上执行的镜像文件，
可以在使用如下方法拉取已经定制好的镜像,首先进入zvm_workspace目录：

.. code:: shell

    cd  ~/zvm_workspace
    git clone https://gitee.com/hnu-esnl/zvm_vm_image.git

复制zvm_vm_image/qemu_max_smp/zvmv37/目录下内容至zvm/zvm_config/qemu_platform/hub/目录：

.. code:: shell

    cp -r zvm_vm_image/qemu_max_smp/zvmv37/. zvm/zvm_config/qemu_platform/hub/


此时，在zvm_config/qemu_platform/hub目录下有Linux和zephyr虚拟机的镜像，直接执行如下命令即可运行：

.. code:: shell

   ./auto_zvm.sh debugserver qemu_max_smp


3. QEMU 平台使用zvm启动虚拟机
-------------------------------

运行zvm平台后可见以下内容：

.. figure:: https://gitee.com/openeuler/zvm/raw/master/zvm_doc/figure/zvm_qemu.png
   :align: center



在zvm窗口上输入如下命令查看平台支持的指令：

.. code:: shell

   zvm help

启动Zephyr客户机
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

（1）创建Zephyr客户机:
+++++++++++++++++++++++++++++

   .. code:: shell

      zvm new -t zephyr


（2）运行Zephyr客户机:
+++++++++++++++++++++++++++++

   .. code:: shell

      zvm run -n 0

（3）进入Zephyr客户机终端:
+++++++++++++++++++++++++++++

   .. code:: shell

      zvm look 0

（4）退出Zephyr客户机终端:
+++++++++++++++++++++++++++++

   .. code:: shell

      [ctrl + x]

(-n后面的数是客户机的对应ID，假设创建所得客户机的VM-ID：0)


启动Linux客户机
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

（1）创建Linux客户机:
+++++++++++++++++++++++++++++

   .. code:: shell

      zvm new -t linux


（2）运行Linux客户机:
+++++++++++++++++++++++++++++

   .. code:: shell

      zvm run -n 2

（3）进入Linux客户机终端:
+++++++++++++++++++++++++++++

   .. code:: shell

      zvm look 2

（4）退出Linux客户机终端:
+++++++++++++++++++++++++++++

   .. code:: shell

      [ctrl + x]

(-n后面的数是客户机的对应ID，假设创建所得客户机的VM-ID：2)


成功运行
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. figure:: https://gitee.com/openeuler/zvm/raw/master/zvm_doc/figure/Run%20successfully.png
   :align: center


`Prev>> 主机开发环境搭建 <https://gitee.com/openeuler/zvm/blob/master/zvm_doc/1_主机开发环境构建.rst>`__
