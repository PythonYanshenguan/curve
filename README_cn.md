[English version](README.md)


<img src="docs/images/curve-logo1.png"/>

# Curve

[![Jenkins Coverage](https://img.shields.io/jenkins/coverage/cobertura?jobUrl=http%3A%2F%2F59.111.91.248%3A8080%2Fjob%2Fcurve_untest_job%2F)](http://59.111.91.248:8080/job/curve_untest_job/HTML_20Report/)
[![Robot failover](https://img.shields.io/jenkins/build?jobUrl=http%3A%2F%2F59.111.91.248%3A8080%2Fjob%2Fcurve_failover_testjob%2F&label=failover)](http://59.111.91.248:8080/job/curve_failover_testjob/)
[![Robot interface](https://img.shields.io/jenkins/tests?jobUrl=http%3A%2F%2F59.111.91.248%3A8080%2Fjob%2Fcurve_robot_job%2F)](http://59.111.91.248:8080/job/curve_robot_job/)
[![BUILD Status](https://img.shields.io/jenkins/build?jobUrl=http%3A%2F%2F59.111.91.248%3A8080%2Fjob%2Fopencurve_multijob%2F)](http://59.111.91.248:8080/job/opencurve_multijob/lastBuild)
[![Docs](https://img.shields.io/badge/docs-latest-green.svg)](https://github.com/opencurve/curve/tree/master/docs)
[![Releases](https://img.shields.io/github/v/release/opencurve/curve?include_prereleases)](https://github.com/opencurve/curve/releases)
[![LICENSE](https://img.shields.io/badge/licence-Apache--2.0%2FGPL-blue)](https://github.com/opencurve/curve/blob/master/LICENSE)


Curve是网易自主设计研发的高性能、易运维、云原生的分布式存储系统，目前提供块(CurveBS)和文件(CurveFS)两种存储方式。CurveBS支持快照克隆和恢复,支持QEMU虚拟机和物理机NBD设备两种挂载方式。CurveFS基于Fuse支持POSIX文件系统接口。

## Curve Block Service vs Ceph Block Device

Curve: v1.2.0

Ceph: L/N
### 性能
块存储场景下，Curve随机读写性能远优于Ceph。
测试环境：6台服务器*20块SATA SSD，E5-2660 v4，256G，3副本，使用nbd场景。

单卷场景：
<image src="docs/images/1-nbd.jpg">

多卷场景：
<image src="docs/images/10-nbd.jpg">

### 稳定性
块存储场景下，常见异常Curve的稳定性优于Ceph。
| 异常场景 | 单盘故障 | 慢盘 | 机器宕机 | 机器卡住 |
| :----: | :----: | :----: | :----: | :----: |
| Ceph | 抖动7s | 持续io抖动 | 抖动7s | 不可恢复 |
| Curve | 抖动4s | 无影响 | 抖动4s | 抖动4s |
### 运维
块存储场景下，Curve常见运维更友好。
| 运维场景 | 客户端升级 | 均衡 |
| :----: | :----: | :----: |
| Ceph | 不支持热升级 | 外部插件调整，影响业务IO |
| Curve | 支持热升级，秒级抖动 | 自动均衡，对业务IO无影响 |

## 设计文档

- 通过 [Curve概述](https://opencurve.github.io/) 可以了解 Curve 架构
- CurveBS相关文档
  - [NEBD](docs/cn/nebd.md)
  - [MDS](docs/cn/mds.md)
  - [Chunkserver](docs/cn/chunkserver_design.md)
  - [Snapshotcloneserver](docs/cn/snapshotcloneserver.md)
  - [Curve质量体系介绍](docs/cn/quality.md)
  - [Curve监控体系介绍](docs/cn/monitor.md)
  - [Client](docs/cn/curve-client.md)
  - [Client Python API](docs/cn/curve-client-python-api.md)
- CurveBS上层应用
  - [对接k8s文档](docs/cn/k8s_csi_interface.md)
- CurveFS相关文档
  - [架构设计](https://github.com/opencurve/curve-meetup-slides/blob/main/CurveFS/CurveFS%E6%96%B9%E6%A1%88%E8%AE%BE%E8%AE%A1%EF%BC%88%E6%80%BB%E4%BD%93%E8%AE%BE%E8%AE%A1%EF%BC%8C%E5%8F%AA%E5%AE%9E%E7%8E%B0%E4%BA%86%E9%83%A8%E5%88%86%EF%BC%89.pdf)
  - [Client概要设计](https://github.com/opencurve/curve-meetup-slides/blob/main/CurveFS/CurveFS%20Client%20%E6%A6%82%E8%A6%81%E8%AE%BE%E8%AE%A1.pdf)
  - [元数据管理](https://github.com/opencurve/curve-meetup-slides/blob/main/CurveFS/Curve%E6%96%87%E4%BB%B6%E7%B3%BB%E7%BB%9F%E5%85%83%E6%95%B0%E6%8D%AE%E7%AE%A1%E7%90%86.pdf)
  - [数据缓存方案](https://github.com/opencurve/curve-meetup-slides/blob/main/CurveFS/Curve%E6%94%AF%E6%8C%81S3%20%E6%95%B0%E6%8D%AE%E7%BC%93%E5%AD%98%E6%96%B9%E6%A1%88.pdf)
  - [空间分配方案](https://github.com/opencurve/curve-meetup-slides/blob/main/CurveFS/Curve%E6%96%87%E4%BB%B6%E7%B3%BB%E7%BB%9F%E7%A9%BA%E9%97%B4%E5%88%86%E9%85%8D%E6%96%B9%E6%A1%88.pdf)
  - [更多](https://github.com/opencurve/curve-meetup-slides/tree/main/CurveFS)

## CurveBS快速开始
为了提升 Curve 的运维便利性，我们设计开发了 [CurveAdm](https://github.com/opencurve/curveadm) 项目，其主要用于部署和管理 Curve 集群，目前已支持部署CurveBS & CurveFS（扩容、版本升级等更多功能正在开发中），相关使用文档请参考 [CurveAdm用户手册](https://github.com/opencurve/curveadm/wiki)，并根据手册首先安装CurveAdm工具之后再进行Curve集群的部署。

### 部署All-in-one体验环境
请参考CurveAdm用户手册中[CurveBS集群部署步骤](https://github.com/opencurve/curveadm/wiki/curvebs-cluster-deployment)，单机体验环境请使用“集群拓扑文件-单机部署”模板。


[单机部署 - 即将废弃方式](docs/cn/deploy.md#%E5%8D%95%E6%9C%BA%E9%83%A8%E7%BD%B2)

### 部署多机集群
请参考CurveAdm用户手册中[CurveBS集群部署步骤](https://github.com/opencurve/curveadm/wiki/curvebs-cluster-deployment)，请使用“集群拓扑文件-多机部署”模板。


[多机部署 - 即将废弃方式](docs/cn/deploy.md#%E5%A4%9A%E6%9C%BA%E9%83%A8%E7%BD%B2)

### 命令行工具说明

[命令行工具说明](docs/cn/curve_ops_tool.md)

## CurveFS快速开始
为了提升 Curve 的运维便利性，我们设计开发了 [CurveAdm](https://github.com/opencurve/curveadm) 项目，其主要用于部署和管理 Curve 集群，目前已支持部署CurveBS & CurveFS（扩容、版本升级等更多功能正在开发中），相关使用文档请参考 [CurveAdm用户手册](https://github.com/opencurve/curveadm/wiki)，并根据手册首先安装CurveAdm工具之后再进行Curve集群的部署。


具体流程见：[CurveFS部署流程](https://github.com/opencurve/curveadm/wiki/curvefs-cluster-deployment)

### 命令行工具说明

[命令行工具说明](curvefs/src/tools#readme)


## 参与开发

### 部署编译开发环境

[编译开发环境搭建](docs/cn/build_and_run.md)

### 测试用例编译及运行
[测试用例编译及运行](docs/cn/build_and_run.md#%E6%B5%8B%E8%AF%95%E7%94%A8%E4%BE%8B%E7%BC%96%E8%AF%91%E5%8F%8A%E6%89%A7%E8%A1%8C)

### FIO curve块存储引擎
fio的curve块存储引擎代码已经上传到 https://github.com/opencurve/fio ，请自行编译测试（依赖nebd库），fio命令行示例：`./fio --thread --rw=randwrite --bs=4k --ioengine=nebd --nebd=cbd:pool//pfstest_test_ --iodepth=10 --runtime=120 --numjobs=10 --time_based --group_reporting --name=curve-fio-test`

### 编码规范
CURVE编码规范严格按照[Google C++开源项目编码指南](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/contents/)来进行代码编写，请您也遵循这一指南来提交您的代码。

### 测试覆盖率要求
1. 单元测试：增量行覆盖80%以上，增量分支覆盖70%以上
2. 集成测试：与单元测试合并统计，满足上述覆盖率要求即可
3. 异常测试：暂不做要求

### 其他开发流程说明
代码开发完成之后，提[pr](https://github.com/opencurve/curve/compare)到curve的master分支。提交pr时，请填写pr模板。pr提交之后会自动触发CI，CI通过并且经过review之后，代码才可合入。
具体规则请见[CONTRIBUTING](https://github.com/opencurve/curve/blob/master/CONTRIBUTING.md).

## 版本发布周期
- CURVE版本发布周期：大版本半年，小版本1~2个月
- 版本号规则：采用3段式版本号，x.y.z{-后缀}，x是大版本，y是小版本，z是bugfix，后缀用来区beta版本(-beta)、rc版本(-rc)、和稳定版本(没有后缀)。每半年的大版本是指x增加1，每1~2个月的小版本是y增加1。正式版本发布之后，如果有bugfix是z增加1。

## 分支规则
所有的开发都在master分支开发，如果需要发布版本，从master拉取新的分支**release-x.y**。版本发布从release-x.y分支发布。

## 反馈及交流

- [Github Issues](https://github.com/openCURVE/CURVE/issues)：欢迎提交BUG、建议，使用中如遇到问题可参考FAQ或加入我们的User group进行咨询
- [FAQ](https://github.com/openCURVE/CURVE/wiki/CURVE-FAQ)：主要根据User group中常见问题整理，还在逐步完善中
- User group：当前为微信群，由于群人数过多，需要先添加以下个人微信，再邀请进群。

<img src="docs/images/curve-wechat.jpeg" style="zoom: 75%;" />






