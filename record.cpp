#include "include/tinyalsa/asoundlib.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <fstream>
#include <semaphore.h>
#include <time.h>
#include <map>
#include <semaphore.h>
#include "record.h"
#include"include/OSAL_log.h"
#define OSAL_LOG_MACRO(_level, _format, _args...)                                                                                                                                                           \
	do                                                                                                                                                                                                      \
	{                                                                                                                                                                                                       \
		if (s_loglevel <= _level)                                                                                                                                                                           \
		{                                                                                                                                                                                                   \
			struct timespec ts;                                                                                                                                                                             \
			clock_gettime(CLOCK_MONOTONIC, &ts);                                                                                                                                                            \
			fprintf(stderr, "" _format "\n", ##_args);                                                                                                                                                      \
			__android_log_print(_level, "TXZRECORD", "[%03u][%ld][TXZ::%s]" _format "[%s:%d]", (uint32_t)((ts.tv_nsec % 1000000000) / 1000000), (long)gettid(), __FUNCTION__, ##_args, __FILE__, __LINE__); \
		}                                                                                                                                                                                                   \
	} while (0)

// enum
// {
// 	OSAL_LOG_DEBUG = ANDROID_LOG_DEBUG,
// 	OSAL_LOG_INFO = ANDROID_LOG_INFO,
// 	OSAL_LOG_WARN = ANDROID_LOG_WARN,
// 	OSAL_LOG_ERROR = ANDROID_LOG_ERROR,
// 	OSAL_LOG_FATAL = ANDROID_LOG_FATAL
// };

// //等级日志宏
// #define LOGD(_format, _args...) OSAL_LOG_MACRO(OSAL_LOG_DEBUG, _format, ##_args)
// #define LOGI(_format, _args...) OSAL_LOG_MACRO(OSAL_LOG_INFO, _format, ##_args)
// #define LOGW(_format, _args...) OSAL_LOG_MACRO(OSAL_LOG_WARN, _format, ##_args)
// #define LOGE(_format, _args...) OSAL_LOG_MACRO(OSAL_LOG_ERROR, _format, ##_args)
// #define LOGF(_format, _args...) OSAL_LOG_MACRO(OSAL_LOG_FATAL, _format, ##_args)

#define TXZ_PORT 7186
#define MAXCONNECTNUM 5
#define LISTENQ 5
using namespace std;
static int s_loglevel = OSAL_LOG_DEBUG;
static int s_fdControlPipe[2];
static unsigned int s_size;
void *txz_record(void *date);
void *txz_dealclient(void *date);

static sem_t s_sem;
static bool s_flag = false;
static bool s_runing = false;
class Client
{
public:
	Client(unsigned int size, int fd)
	{
		sem_init(&m_sem, 0, 0);
		buff = new char[size * 500];
		this->fd = fd;
		w = r = buff;
		end = buff + size * 500;
	}
	void clear()
	{
		w = r;
		sem_init(&m_sem, 0, 0);
	}
	int read(char *p, int n)
	{
		int count;
		int poor = w - r;
		if (poor >= 0)
		{
			count = s_size * 500 - poor;
			n = (n >= count ? count : n);
			int len = end - w;
			if (n < len)
			{
				memcpy(w, p, n);
				w = w + n;
			}
			else
			{
				memcpy(w, p, len);
				memcpy(buff, p + len, n - len);
				w = buff + n - len;
			}
		}
		else if (poor < 0)
		{
			count = -poor;
			n = (n >= count ? count : n);
			memcpy(w, p, n);
		}
		return n;
	}
	void write()
	{
		while (m_flag)
		{
			char *cur_w = w;
			int writen = 0;
			while (cur_w - r)
			{
				if (cur_w - r > 0)
				{
					int res = ::write(fd, r, cur_w - r);
					r = r + res;
					writen += res;
					continue;
				}
				else
				{
					int res = ::write(fd, r, end - r);
					r = r + res;
					writen += res;
					if (end - r == 0)
					{
						r = buff;
					}
					continue;
				}
			}
			//LOGD("writen :%d",writen);
			sem_wait(&m_sem);
		}
	}

	void notifyData()
	{
		sem_post(&m_sem);
	}

	void destroy()
	{
		m_flag = false;
	}

	~Client()
	{
		delete[] buff;
		close(fd);
	}
	bool state = true;

private:
	int fd;
	sem_t m_sem;
	bool m_flag = true;
	char *buff = nullptr;
	char *r = nullptr;
	char *w = nullptr;
	char *end = nullptr;
};

static map<int, pair<pthread_t, Client *>> mapClient;

int txz_recordServer_init()
{
	s_runing = true;
	prctl(PR_SET_NAME, "tcp_server");
	LOGD("tcp_server thread id = %ld", (long)gettid());
	int listenfd = 0;
	struct sockaddr_in servaddr;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		LOGD("socket err errno:%d errstr:%s", errno, strerror(errno));
		return -1;
	}
	const int SO_REUSEADDR_on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &SO_REUSEADDR_on, sizeof(SO_REUSEADDR_on));
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(TXZ_PORT);
	socklen_t len = sizeof(servaddr);
	if (::bind(listenfd, (struct sockaddr *)&servaddr, len) == -1)
	{
		LOGD("bind err errno:%d errstr:%s", errno, strerror(errno));
		return -1;
	}
	LOGD("bind ok");

	if (listen(listenfd, LISTENQ) == -1)
	{
		LOGD("listen err errno:%d errstr:%s", errno, strerror(errno));
		return -1;
	}
	LOGD("listen ok");

	fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL) | O_NONBLOCK);

	if (pipe(s_fdControlPipe) < 0)
	{
		LOGE("create pipe error errno:%d errstr:%s", errno, strerror(errno));
		return -1;
	}
	fcntl(s_fdControlPipe[0], F_SETFL, fcntl(s_fdControlPipe[0], F_GETFL) | O_NONBLOCK);

	fd_set client_fdset; //监控文件描述符集合

	int maxsock = listenfd; //监控摁键描述符中最大的文件号
	if (s_fdControlPipe[0] > maxsock)
	{
		maxsock = s_fdControlPipe[0];
	}

	int ret = sem_init(&s_sem, 0, 0);
	if (-1 == ret)
	{
		return 0;
	}

	pthread_t txzRecordTid = 0;
	ret = pthread_create(&txzRecordTid, nullptr, txz_record, nullptr);
	if (!ret)
	{
		LOGD("creat txz_record thread sucess");
	}
	else
	{
		LOGD("creat txz_record thread fail");
		return -1;
	}

	int client_sockfd[MAXCONNECTNUM] = {0};
	char *buff = nullptr;
	while (true)
	{
		if (!s_runing)
		{
			break;
		}
		
		//初始化文件描述符到集合
		FD_ZERO(&client_fdset);
		//加入服务器描述符
		FD_SET(listenfd, &client_fdset);
		FD_SET(s_fdControlPipe[0], &client_fdset);

		for (auto i : mapClient)
		{
			LOGD("mapClient insert select:%d", i.first);
			FD_SET(i.first, &client_fdset);
		}

		int ret = select(maxsock + 1, &client_fdset, NULL, NULL, NULL);
		if (ret < 0)
		{
			if (errno == EAGAIN)
			{
				LOGW("select return error errno:%d errstr:%s", errno, strerror(errno));
				continue;
			}
			else
			{
				LOGE("select return error errno:%d errstr:%s", errno, strerror(errno));
				break;
			}
		}

		if (FD_ISSET(listenfd, &client_fdset)) //描述符可读
		{
			LOGD("listenfd Can be read");
			struct sockaddr_in client_addr; //对端
			socklen_t len = sizeof(struct sockaddr_in);
			int clientfd = accept(listenfd, (struct sockaddr *)&client_addr, &len);
			if (clientfd < 0)
			{
				LOGD("accept err errno:%d errstr:%s", errno, strerror(errno));
			}
			else //把新的连接加入到描述符集中
			{
				LOGD("accept port : %u fd = %d", client_addr.sin_port, clientfd);
				if (mapClient.size() == MAXCONNECTNUM) //达到最大连接数
				{
					close(clientfd);
				}
				fcntl(clientfd, F_SETFL, fcntl(clientfd, F_GETFL) | O_NONBLOCK);
				if (clientfd > maxsock)
				{
					maxsock = clientfd;
				}

				Client *client = new Client(s_size, clientfd);

				pthread_t pid;
				int ret = pthread_create(&pid, nullptr, txz_dealclient, client);
				if (!ret)
				{
					LOGD("creat dealClient thread sucess");
				}
				else
				{
					LOGD("creat dealClient thread fail");
					delete client;
					//mapClient.erase(mapClient.find(clientfd));
				}
				mapClient.emplace(clientfd, pair<pthread_t, Client *>(pid, client));
			}
		}

		for (auto i : mapClient)
		{
			if (FD_ISSET(i.first, &client_fdset)) //描述符可读
			{
				LOGD("fd [%d] Can be read", i.first);
				int ret = 0;
				uint32_t flag = -1;
				do
				{
					int res = read(i.first, &flag + ret, sizeof(uint32_t) - ret);
					if (res <= 0)
					{
						if (errno == EAGAIN)
						{
							continue;
						}
						else
						{
							i.second.second->destroy();
							i.second.second->notifyData();
							void *tmp;
							pthread_join(i.second.first, &tmp);
							delete i.second.second;
							mapClient.erase(mapClient.find(i.first));
							break;
						}
					}
					ret += res;
				} while (ret < sizeof(uint32_t));
				switch (flag)
				{
				case 0:
					i.second.second->state = false;
					i.second.second->clear();
					break;
				case 1:
					i.second.second->state = true;
					break;
				default:
					break;
				}
			}
		}

		if (FD_ISSET(s_fdControlPipe[0], &client_fdset)) //分发录音
		{
			//LOGD("s_fdControlPipe[0] Can be read");
			if (!buff)
			{
				buff = new char[s_size];
			}
			int ret = read(s_fdControlPipe[0], buff, s_size);
			for (auto i : mapClient)
			{
				if (i.second.second->state == true)
				{
					int reedn = i.second.second->read(buff, ret);
					LOGD("readn = %d ", reedn);
					i.second.second->notifyData();
				}
			}
		}
	}

	for (int i = 0; i < MAXCONNECTNUM; ++i)
	{
		if (client_sockfd[i])
		{
			close(client_sockfd[i]);
		}
	}
	if (buff)
		delete[] buff;
	close(s_fdControlPipe[0]);
	close(s_fdControlPipe[1]);
	close(listenfd);
	return 0;
}

void *txz_record(void *date)
{
	prctl(PR_SET_NAME, "txz_record");
	LOGD("txz_record thread id = %ld", (long)gettid());

	ifstream read_file;
	read_file.open("info.txt", ios::binary);

	struct pcm_config config;
	struct pcm *pcm = nullptr;
	char *buffer = nullptr;
	unsigned int card = 0;
	unsigned int device = 0;
	unsigned int channels = 2;
	unsigned int rate = 16000;
	unsigned int bits = 16;
	unsigned int frames;
	unsigned int period_size = 1024;
	unsigned int period_count = 4;

	enum pcm_format format = PCM_FORMAT_S16_LE;
	string line;
	while (getline(read_file, line))
	{
		auto found = line.find("-D:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			card = (unsigned int)atoi(line.c_str());
			LOGD("card = %u", card);
			line.clear();
			continue;
		}
		found = line.find("-d:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			device = (unsigned int)atoi(line.c_str());
			LOGD("device = %u", device);
			line.clear();
			continue;
		}
		found = line.find("-c:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			channels = (unsigned int)atoi(line.c_str());
			LOGD("channels = %u", channels);
			line.clear();
			continue;
		}
		found = line.find("-r:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			rate = (unsigned int)atoi(line.c_str());
			LOGD("rate = %u", rate);
			line.clear();
			continue;
		}
		found = line.find("-b:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			bits = (unsigned int)atoi(line.c_str());
			LOGD("bits = %u", bits);
			line.clear();
			continue;
		}
		found = line.find("-p:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			period_size = (unsigned int)atoi(line.c_str());
			LOGD("period_size = %u", period_size);
			line.clear();
			continue;
		}
		found = line.find("-n:");
		if (found != std::string::npos)
		{
			string str = line.erase(0, 3);
			LOGD("find :%u str :%s line: %s", found, str.c_str(), line.c_str());
			period_count = (unsigned int)atoi(line.c_str());
			LOGD("period_count = %u", period_count);
			line.clear();
			continue;
		}
	}

	switch (bits)
	{
	case 32:
		format = PCM_FORMAT_S32_LE;
		break;
	case 24:
		format = PCM_FORMAT_S24_LE;
		break;
	case 16:
		format = PCM_FORMAT_S16_LE;
		break;
	default:
		LOGD("%d bits is not supported.\n", bits);
		return nullptr;
	}

	config.channels = channels;
	config.rate = rate;
	config.period_size = period_size;
	config.period_count = period_count;
	config.format = format;
	config.start_threshold = 0;
	config.stop_threshold = 0;
	config.silence_threshold = 0;

	pcm = pcm_open(card, device, PCM_IN, &config);
	if (!pcm || !pcm_is_ready(pcm))
	{
		LOGE("Unable to open PCM device (%s)\n", pcm_get_error(pcm));
		return nullptr;
	}
	s_size = pcm_get_buffer_size(pcm); //10ms
									   //size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	buffer = new char[s_size];
	if (!buffer)
	{
		LOGE("Unable to allocate %d bytes\n", s_size);
		pcm_close(pcm);
		return nullptr;
	}

	LOGD("Capturing sample: %u ch, %u hz, %u bit 10ms size:%u\n", config.channels, config.rate, format, s_size);

	// FILE* fp = fopen("/sdcard/txz_read.pcm","wb+");
	// if(fp)
	// {
	// 	LOGD("txz_read.pcm creat success");
	// }
	while (true)
	{
		if (!s_runing)
		{
			break;
		}
		
		if (s_flag == false)
		{
			if (!sem_wait(&s_sem))
			{
				LOGD("server start");
			}
			else
			{
				LOGD("sem_wait err");
				return nullptr;
			}
		}
		int res = pcm_read(pcm, buffer, s_size);
		int ret = write(s_fdControlPipe[1], buffer, s_size);
		// fwrite(buffer,s_size,1,fp);
		// fflush(fp);
		//LOGD("res = %d ret = %d",res,ret);
	}
	// fclose(fp);
	delete[] buffer;
	pcm_close(pcm);
	return nullptr;
}

void *txz_dealclient(void *date)
{
	prctl(PR_SET_NAME, "txz_dealclient");
	LOGD("txz_dealclient thread id = %ld", (long)gettid());
	Client *client = (Client *)date;
	client->write();
	LOGD("txz_dealclient thread id = %ld exit", (long)gettid());
	pthread_exit((void *)0);
}

void txz_recordServer_start()
{
	sem_post(&s_sem);
	s_flag = true;
}

void txz_recordServer_stop()
{
	s_flag = false;
}

void txz_recordServer_destroy()
{
	s_runing = false;
}