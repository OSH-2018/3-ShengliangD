## SFFS

* 存储空间以 4k 为块大小划分；

* block 0

  文件系统元信息

  * 块大小
  * 块数
  * 已占用块数

* bit map blocks

  各个块是否占用，每个 bit 对应一个 block

* index blocks

  文件首块的列表，每个 unsigned long long 对应一个文件首块号

* data blocks

  * 第一块

    文件的元数据

    * 文件名（1024）
    * 文件类型
    * 修改时间（不含创建时间）
    * 大小

  * 其他块

    文件的数据

## 要求

- [x] 创建

- [ ] 读

- [ ] 写

  目前假定 offset 为文件内的有效偏移

- [ ] 截断

- [ ] 修改元数据

- [ ] 错误处理

- [ ] 文件元数据维护

- [ ] 空间不足的判断

## 参数说明

