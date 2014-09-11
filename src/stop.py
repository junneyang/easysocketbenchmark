#-*- coding: utf-8 -*-
#!/usr/bin/env python

import os
import signal
import commands

cmd="ps -ef | grep ./redismaintest | grep -v grep | awk -F' ' '{ print $2 }'"
status,output=commands.getstatusoutput(cmd)
#发送信号，16175是前面那个绑定信号处理函数的pid，需要自行修改
os.kill(int(output),signal.SIGINT)
