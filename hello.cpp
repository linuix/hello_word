#include <stdio.h>
#include <unistd.h>
#include "include/OSAL_log.h"
#include "exec_cmd.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "record.h"

#include "include/tinyalsa/asoundlib.h"


void sig_usr(int sig)
{
    printf("sig %d\n",sig);
    int res = wait(nullptr);
    printf("res1 = %d\n",res);
    return;
}

void *txz_record_init(void*){
    sleep(6);
    txz_recordServer_init();
}

int main(){
    printf("---------------------start--------------\n");
    // pthread_t txzRecordTid = 0;

    // int preState = -1,currentStat = 0;

    // int pid = -1;

    // while (1)
    // {
    //     int rec = access("/dev/snd/pcmC1D0c",0);
    //     printf("----------rec = %d\n",rec);
    //     LOGD("----------rec = %d\n",rec);
    //     currentStat = rec;
    //     if (rec == 0 && preState!=currentStat)
    //     {//插入U盘
           
    //         printf("---------------------------u path insert---------------------------\n");
    //         LOGD("---------------------------u path insert---------------------------\n");
    //         __pid_t res = fork();
    //         if(res > 0)
    //         {
    //             preState = currentStat;
    //             pid = res;
    //             printf("res =  %d-------------------------\n",res);
    //             //监听信号
    //             signal(SIGCHLD,sig_usr);
    //             int ret = pthread_create(&txzRecordTid, nullptr, txz_record_init, nullptr);
    //             LOGD("pthread_create = ret",ret);
    //             txz_recordServer_start();

    //             if (!ret)
    //             {
    //                 LOGD("creat txz_record thread sucess");
    //             }
    //             else
    //             {
    //                 LOGD("creat txz_record thread fail");
    //                 return -1;
    //             }
    //             continue;
    //         }
    //         else if (res == 0)
    //         {
    //             pid = getpid();
    //             printf("pid =  %d-------------------------\n",pid);
    //             execl("/data/local/rx5/unicensd","unicensd","/data/local/rx5/config-iconiq-surround.xml",NULL);
    //             // execl("system/bin/sh","sh","/data/local/rx5/startRecord.sh",NULL);
    //             // readCommand("/data/local/rx5/unicensd /data/local/rx5/config-iconiq-surround.xml");
    //             printf("-------------------------exec error pid =  %d\n",pid);
    //             exit(0);
    //         }
    //         else 
    //         {
    //             LOGD("fork error\n");
    //             printf("fork error\n");
    //             return 0;
    //         }

    //     }
    //     else if (rec == -1 && preState!=currentStat)
    //     {//拔出U盘
    //         printf("---------------------------u path uninsert---------------------------\n");
    //         LOGD("---------------------------u path uninsert---------------------------\n");
    //         if(pid != -1){
    //             printf("kill pid = %d\n",pid);
    //             txz_recordServer_destroy();
    //             // txz_recordServer_stop();
            
    //             kill(pid,SIGKILL);
    //         }
    //     }
    //     preState = currentStat;
    //     sleep(5);
    
    // }

    pcm_config pcmConfig;
    pcm *pcm;
    
    pcmConfig.channels = 1;
    pcmConfig.format = PCM_FORMAT_S16_LE;
    pcmConfig.rate = 16000;
    pcmConfig.period_count = 2;
    pcmConfig.period_size = 1024;

    pcmConfig.start_threshold = 0;
	pcmConfig.stop_threshold = 0;
	pcmConfig.silence_threshold = 0;
    
    pcm = pcm_open(0,0, PCM_IN ,&pcmConfig);

    if (nullptr == pcm || !pcm_is_ready(pcm))
    {
        printf("pcm open failed !\n");
        return -1;
    }
    printf("pcm open success !\n");
    size_t pcm_size = pcm_get_buffer_size(pcm);

    char *pcmData = new char[pcm_size];
    printf("buffer size %d\n",pcm_size);
    int rec = 0;
    FILE *f;
    f = fopen("/sdcard/pcmRecord.pcm","rwb+");
    int count = 0;
    while ((rec = pcm_read(pcm,pcmData,pcm_size))==0)
    {
        printf("read data length = %d\n",rec);
        fwrite(pcmData,rec,1,f);
        if (count > 1024*100)
        {
            break;
        }
        
    }
    printf("read data length = %d\n",rec);

    
    
    fclose(f);
    pcm_close(pcm);
    
    
 
    return 0;



}