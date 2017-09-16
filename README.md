# mp3-player
基于mini6410的SD卡MP3播放器设计与实现

app：主应用程序

driver：按键驱动

song：mp3歌曲播放列表
  
bin：可用于测试的mp3播放器程序与按键驱动


主要功能介绍：
自动循环播放SD卡中的mp3歌曲，并可实现MP3播放器功能。
K1:播放、暂停
 			 
K2:停止播放
 			 
K3:上一首
 			 
K4:下一首
 

平台运行环境：
mini6410/tiny6410开发板
Linux-2.6.38内核

实验步骤：、
1、拷贝int_key_drv.ko、mp3_player、song歌曲到SD卡，上电开发板，插上SD卡。
2、添加按键驱动 int_key_drv.ko
3、创建按键设备文件节点  /dev/key
4、运行mp3播放器主控程序mp3_player
