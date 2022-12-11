# Http Server

### 功能
- 服务端：接受浏览器访问，返回对应的文件

### 流程
- 浏览器请求文件存在：返回文件
- 浏览器请求非法/文件不存在：返回错误信息

### 运行结果
```text


server:
    Server is running!
    [socket 5] connect
    [socket 6] connect
    [socket 7] connect
    [Bob]: Hello
    [Alice]: Hi!
    [Tom]: Great
    [socket 5] disconnect
    [socket 6] disconnect
    [socket 7] disconnect
```