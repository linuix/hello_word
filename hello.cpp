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
    pthread_t txzRecordTid = 0;

    int preState = -1,currentStat = 0;

    int pid = -1;
    int ret = pthread_create(&txzRecordTid, nullptr, txz_record_init, nullptr);
    LOGD("pthread_create = ret",ret);
    sleep(4);

    while (1)
    {
        int rec = access("/dev/snd/pcmC1D0c",0);
        printf("----------rec = %d\n",rec);
        LOGD("----------rec = %d\n",rec);
        currentStat = rec;
        if (rec == 0 && preState!=currentStat)
        {//插入U盘
           
            printf("---------------------------u path insert---------------------------\n");
            LOGD("---------------------------u path insert---------------------------\n");
            __pid_t res = fork();
            if(res > 0)
            {
                preState = currentStat;
                pid = res;
                printf("res =  %d-------------------------\n",res);
                //监听信号
                signal(SIGCHLD,sig_usr);

                txz_recordServer_start();

                if (!ret)
                {
                    LOGD("creat txz_record thread sucess");
                }
                else
                {
                    LOGD("creat txz_record thread fail");
                    return -1;
                }
                continue;
            }
            else if (res == 0)
            {
                pid = getpid();
                printf("pid =  %d-------------------------\n",pid);
                execl("/data/local/rx5/unicensd","unicensd","/data/local/rx5/config-iconiq-surround.xml",NULL);
                // execl("system/bin/sh","sh","/data/local/rx5/startRecord.sh",NULL);
                // readCommand("/data/local/rx5/unicensd /data/local/rx5/config-iconiq-surround.xml");
                printf("-------------------------exec error pid =  %d\n",pid);
                exit(0);
            }
            else 
            {
                LOGD("fork error\n");
                printf("fork error\n");
                return 0;
            }

        }
        else if (rec == -1 && preState!=currentStat)
        {//拔出U盘
            printf("---------------------------u path uninsert---------------------------\n");
            LOGD("---------------------------u path uninsert---------------------------\n");
            if(pid != -1){
                printf("kill pid = %d\n",pid);
                txz_recordServer_stop();
                kill(pid,SIGKILL);
            }
        }
        preState = currentStat;
        sleep(5);
    
    }

}