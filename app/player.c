/*
 *     SD卡mp3播放器控制程序
 *	   功能：
 			 k1:播放、暂停
 			 k2:停止播放
 			 k3:上一首
 			 k4:下一首
 *     附加：歌曲自动循环播放SD卡/sdcard/song目录中mp3歌曲
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/*共享内存申请标记*/
#define PERM S_IRUSR|S_IWUSR													

/*双向循环列表：存放歌曲名*/
struct song				
{
	char songname[20];
	struct song *prev;
	struct song *next;
};

/*孙子进程id号*/
pid_t gradchild;

/*子进程id号*/
pid_t pid;

/*共享内存描述标记*/
int shmid;
char *p_addr;
/*共享内存内容格式*/
/*|gradchild(孙子进程PID) |+ |空一个字节|+ currentsong(当前播放列表的节点指针)|*/

/*播放标记*/
int first_key=1;
int play_flag=0;

/*************************************************
Function name: play
Parameter    : struct song *
Description	 : 播放函数
Return		 : void
Argument     : void
Autor & date : ada 09,12,07
**************************************************/
void play(struct song *currentsong)
{
	pid_t fd;
	char *c_addr;
	char *p;
	int len;
	char my_song[30]="/sdcard/song/";
	while(currentsong)
	{
		/*创建子进程，即孙子进程*/
		fd = fork();
		if(fd == -1)
		{	
			perror("fork");
			exit(1);
		}
		else if(fd == 0) //孙子进程
		{
			/*把歌曲名加上根路径*/
			strcat(my_song,currentsong->songname);
			p = my_song;
			len = strlen(p);

			/*去掉文件名最后的'\n'*/
			my_song[len-1]='\0';
			printf("**********基于mini6410/tiny6410开发板SD卡MP3播放器系统*********** \n");
			printf("********************嵌入式家园: -www.embedclub.com ********************\n");
			printf("******上海嵌入式家园-开发板商城:  http://embedclub.taobao.com/*******\n");

			printf("THIS SONG IS %s\n",my_song);
                      /*运行madplay播放器，播放MP3*/
			execl("/usr/bin/madplay","madplay",my_song,NULL);//播放当前列表MP3音乐
			printf("\n\n\n");
		}
		else //子进程
		{
			/*内存映射*/
			c_addr = shmat(shmid,0,0);

			/*把孙子进程的id和当前播放歌曲的节点指针传入共享内存*/
			memcpy(c_addr,&fd,sizeof(pid_t));
			memcpy(c_addr + sizeof(pid_t)+1,&currentsong,4);
			/*使用wait阻塞子进程，直到孙子进程播放完才能被唤醒；
			  当被唤醒时，表示播放MP3期间没有按键按下，则继续顺序播放下一首MP3*/
			if(fd == wait(NULL))
			{
				currentsong = currentsong->next;
				printf("THE NEXT SONG IS %s\n",currentsong->songname);
			}
		}
	}
}

/*************************************************
Function name: creat_song_list
Parameter    : void
Description	 : 创建歌曲名的双向循环链表
Return		 : struct song *
Argument     : void
Autor & date : ada 09.12.07
**************************************************/
struct song *creat_song_list(void)
{	
	FILE *fd;
	size_t size;
	size_t len;
	char *line = NULL;
	struct song *head;
	struct song *p1;
	struct song *p2;
	system("ls /sdcard/song >song_list");
	fd = fopen("song_list","r");

	p1 = (struct song *)malloc(sizeof(struct song));

	printf("==================================song list=====================================\n");
	system("ls /sdcard/song");	
	printf("\n");
	printf("================================================================================\n");
	size = getline(&line,&len,fd);

	strncpy(p1->songname,line,strlen(line));
	head = p1;
	while((size = getline(&line,&len,fd)) != -1) //从文件中读取一行，直到出错或者到文件尾EOF返回-1
	{	
		p2 = p1;
		p1 = (struct song *)malloc(sizeof(struct song));
		strncpy(p1->songname,line,strlen(line));
		p2->next = p1;
		p1->prev = p2;	
	}
	p1->next = head;
	head->prev = p1;
	p1 = NULL;
	p2 = NULL;
	system("rm -rf song_list");
	return head;
}
/*************************************************
Function name: startplay
Parameter    : pid_t *，struct song *
Description	 : 开始播放函数
Return		 : void
Argument     : void
Autor & date : ada 09.12.07
**************************************************/
void startplay(pid_t *childpid,struct song *my_song)
{
	pid_t pid;
	int ret;
	/*创建子进程*/
	pid = fork();

	if(pid > 0) //父进程
	{
		*childpid = pid; //子进程PID初始化
		play_flag = 1;
		sleep(1);
		/*读取共享内存保存的pid,初始化孙子进程的pid*/
		memcpy(&gradchild,p_addr,sizeof(pid_t));
	}
	else if(0 == pid) //子进程
	{	
		/*子进程播放MP3函数*/
		play(my_song);
	}
}
/*************************************************
Function name: my_pause
Parameter    : pid_t
Description	 : 暂停函数
Return		 : void
Argument     : void
Autor & date : ada 09,12,07
**************************************************/
void my_pause(pid_t pid)
{
	printf("=======================PAUSE!PRESS K1 TO CONTINUE===================\n");
	kill(pid,SIGSTOP); //对孙子进程发送SIGSTOP信号
	play_flag = 0;
}

/*************************************************
Function name: my_pause
Parameter    : pid_t
Description	 : 停止播放函数
Return		 : void
Argument     : void
Autor & date : ada 09,12,07
**************************************************/
void my_stop(pid_t g_pid)
{

	printf("=======================STOP!PRESS K1 TO START PLAY===================\n");
	kill(g_pid,SIGKILL); //对孙子进程发送SIGKILL信号
	kill(pid,SIGKILL);   //对子进程发送SIGKILL信号
	first_key=1;

}

/*************************************************
Function name: conti_play
Parameter    : pid_t
Description	 : 继续函数
Return		 : void
Argument     : void
Autor & date : ada 09,12,07
**************************************************/
void conti_play(pid_t pid)
{
	printf("===============================CONTINUE=============================\n");
	kill(pid,SIGCONT); //对孙子进程发送SIGCONT信号
	play_flag=1;
}

/*************************************************
Function name: next
Parameter    : pid_t
Description	 : 下一首函数
Return		 : void
Argument     : void
Autor & date : ada 09.12.07
**************************************************/
void next(pid_t next_pid)
{
	struct song *nextsong;

	printf("===============================NEXT MP3=============================\n");
	/*从共享内存获得孙子进程播放歌曲的节点指针*/
	memcpy(&nextsong,p_addr + sizeof(pid_t)+1,4);
	/*指向下首歌曲的节点*/
	nextsong = nextsong->next;
	/*杀死当前歌曲播放的子进程，孙子进程*/
	kill(pid,SIGKILL);
	kill(next_pid,SIGKILL);
	wait(NULL);
	startplay(&pid,nextsong);
}

/*************************************************
Function name: prev
Parameter    : pid_t
Description	 : 上一首函数
Return		 : void
Argument     : void
Autor & date : yuanhui 09.12.08
**************************************************/
void prev(pid_t prev_pid)
{
	struct song *prevsong;
	/*从共享内存获得孙子进程播放歌曲的节点指针*/
	printf("===============================PRIOR MP3=============================\n");
	memcpy(&prevsong,p_addr + sizeof(pid_t)+1,4);
	/*指向上首歌曲的节点*/
	prevsong = prevsong->prev;
	/*杀死当前歌曲播放的子进程，孙子进程*/
	kill(pid,SIGKILL);
	kill(prev_pid,SIGKILL);
	wait(NULL);
	startplay(&pid,prevsong);
}

/*************************************************
Function name: main
Parameter    : void
Description	 : 主函数
Return		 : int
Argument     : void
Autor & date : ada 09.12.07
**************************************************/
int main(void)
{
	int buttons_fd;
	int key_value;
	struct song *head;
	/*打开设备文件*/
	buttons_fd = open("/dev/key", 0);
	if (buttons_fd < 0) {
		perror("open device buttons");
		exit(1);
	}

	printf("**********基于mini6410/tiny6410开发板SD卡MP3播放器系统*********** \n");
	printf("********************嵌入式家园: -www.embedclub.com ********************\n");
	printf("******上海嵌入式家园-开发板商城:  http://embedclub.taobao.com/*******\n");
	
  /*创建播放列表*/
	head = creat_song_list();
	
	printf("===================================FUNTION======================================\n\n");
	printf("        K1:START/PAUSE     K2:STOP   K3:NEXT      K4:PRIOR\n\n");
	printf("================================================================================\n");


  /*共享内存：用于存放子进程ID，播放列表位置*/
	if((shmid = shmget(IPC_PRIVATE,5,PERM))== -1)
		exit(1);
	p_addr = shmat(shmid,0,0);
	memset(p_addr,'\0',1024);
	
	
	while(1) 
	{
		fd_set rds;
		int ret;

		FD_ZERO(&rds);
		FD_SET(buttons_fd, &rds);

		/*监听获取键值*/
		ret = select(buttons_fd + 1, &rds, NULL, NULL, NULL);
		if (ret < 0) 
		{
			perror("select");
			exit(1);
		}
		if (ret == 0) 
			printf("Timeout.\n");
		else if (FD_ISSET(buttons_fd, &rds))
		{
			int ret = read(buttons_fd, &key_value, sizeof key_value);
			if (ret != sizeof key_value) 
			{
				if (errno != EAGAIN)
					perror("read buttons\n");
				continue;
			} 
			else
			{
				printf("buttons_value: %d\n", key_value+1);
				
				/*首次播放，必须是按键1*/
				if(first_key){
					switch(key_value)
					{	
					case 0:
						startplay(&pid,head);
						first_key=0;
						break;
					case 1:
					case 2:
					case 3:
						printf("=======================PRESS K1 TO START PLAY===================\n");
						break;
				    default:
						printf("=======================PRESS K1 TO START PLAY===================\n");
						break;
					} //end switch
				}//end if(first_key)
				/*若不是首次播放，则根据不同键值处理*/
				else if(!first_key){
				    switch(key_value)
					{
					case 0:
						//printf("play_flag:%d\n",play_flag);
						if(play_flag)
							my_pause(gradchild);
						else
							conti_play(gradchild);
						break;
					case 1:
						my_stop(gradchild);
						break;
					case 2:
						next(gradchild);
						break;
					case 3:
						prev(gradchild);
						break;
					} //end switch
			 }//end if(!first_key)

			}
				
		}
	}

	close(buttons_fd);
	return 0;
}

