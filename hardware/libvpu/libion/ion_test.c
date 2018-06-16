#define LOG_TAG "iontest"

//#include <cutils/log.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ion/ion.h"
#include "linux/ion.h"
//#include <linux/omap_ion.h>
#include <sys/time.h>
#include <unistd.h>

size_t len = 1024*1024, align = 0;
int prot = PROT_READ | PROT_WRITE;
int map_flags = MAP_SHARED;
int alloc_flags = 0;
int heap_mask = 1;
int test = -1;
size_t stride;
int verbose = 0;

/*
#define PRINTV(...)  \
	if(verbose) { \
		printf(); \
	}
*/

int _ion_alloc_test(int *fd, ion_user_handle_t *handle)
{
	int ret;

	*fd = ion_open();
	if (*fd < 0)
		return *fd;

	ret = ion_alloc(*fd, len, align, heap_mask, alloc_flags, handle);

	if (ret)
		printf("%s failed: %s\n", __func__, strerror(ret));
	return ret;
}

void ion_alloc_test()
{
	int fd, ret;
    ion_user_handle_t handle;

	if(_ion_alloc_test(&fd, &handle))
			return;

	ret = ion_free(fd, handle);
	if (ret) {
		printf("%s failed: %s %d\n", __func__, strerror(ret), handle);
		return;
	}
	ion_close(fd);
	printf("ion alloc test: passed\n");
}

void ion_map_test()
{
	int fd, map_fd, ret;
	size_t i;
    ion_user_handle_t handle;
	unsigned char *ptr;

	if(_ion_alloc_test(&fd, &handle))
		return;

	ret = ion_map(fd, handle, len, prot, map_flags, 0, &ptr, &map_fd);
	if (ret)
		return;

	for (i = 0; i < len; i++) {
		ptr[i] = (unsigned char)i;
	}
	for (i = 0; i < len; i++)
		if (ptr[i] != (unsigned char)i)
			printf("%s failed wrote %d read %d from mapped "
			       "memory\n", __func__, i, ptr[i]);
	/* clean up properly */
	ret = ion_free(fd, handle);
	ion_close(fd);
	munmap(ptr, len);
	close(map_fd);

	_ion_alloc_test(&fd, &handle);
	close(fd);

}

void ion_share_test()

{
    ion_user_handle_t handle;
    int sd[2];
    int num_fd = 1;
    struct iovec count_vec = {
        .iov_base = &num_fd,
        .iov_len = sizeof num_fd,
    };
    char buf[CMSG_SPACE(sizeof(int))];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sd);
    if (fork()) {
        struct msghdr msg = {
            .msg_control = buf,
            .msg_controllen = sizeof buf,
            .msg_iov = &count_vec,
            .msg_iovlen = 1,
        };

		struct cmsghdr *cmsg;
		int fd, share_fd, ret;
		char *ptr;
		/* parent */
		if(_ion_alloc_test(&fd, &handle))
			return;
		ret = ion_share(fd, handle, &share_fd);
		if (ret)
			printf("share failed %s\n", strerror(errno));
		ptr = mmap(NULL, len, prot, map_flags, share_fd, 0);
		if (ptr == MAP_FAILED) {
			return;
		}
		strcpy(ptr, "master");
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		*(int *)CMSG_DATA(cmsg) = share_fd;
		/* send the fd */
		printf("master? [%10s] should be [master]\n", ptr);
		printf("master sending msg 1\n");
		sendmsg(sd[0], &msg, 0);
		if (recvmsg(sd[0], &msg, 0) < 0)
			perror("master recv msg 2");
		printf("master? [%10s] should be [child]\n", ptr);

		/* send ping */
		sendmsg(sd[0], &msg, 0);
		printf("master->master? [%10s]\n", ptr);
		if (recvmsg(sd[0], &msg, 0) < 0)
			perror("master recv 1");
	} else {
		struct msghdr msg;
		struct cmsghdr *cmsg;
		char* ptr;
		int fd, recv_fd;
		char* child_buf[100];
		/* child */
		struct iovec count_vec = {
			.iov_base = child_buf,
			.iov_len = sizeof child_buf,
		};

		struct msghdr child_msg = {
			.msg_control = buf,
			.msg_controllen = sizeof buf,
			.msg_iov = &count_vec,
			.msg_iovlen = 1,
		};

		if (recvmsg(sd[1], &child_msg, 0) < 0)
			perror("child recv msg 1");
		cmsg = CMSG_FIRSTHDR(&child_msg);
		if (cmsg == NULL) {
			printf("no cmsg rcvd in child");
			return;
		}
		recv_fd = *(int*)CMSG_DATA(cmsg);
		if (recv_fd < 0) {
			printf("could not get recv_fd from socket");
			return;
		}
		printf("child %d\n", recv_fd);
		fd = ion_open();
		ptr = mmap(NULL, len, prot, map_flags, recv_fd, 0);
		if (ptr == MAP_FAILED) {
			return;
		}
		printf("child? [%10s] should be [master]\n", ptr);
		strcpy(ptr, "child");
		printf("child sending msg 2\n");
		sendmsg(sd[1], &child_msg, 0);
	}
}

enum cmd_index {
	ION_OPEN = 0,
	ION_CLOSE,
	ION_ALLOC,
	ION_FREE,
	ION_MAP,
	ION_SHARE,
	ION_ALLOC_FD,
	ION_IMPORT,
	ION_SYNC_FD,
	ION_GET_PHYS,
	SYS_MMAP,
	SYS_MUNMAP,
	SHARE_CLOSE,
	DATA_CHECK,
	DATA_READ,
	DATA_WRITE,
	PRINT,
	SYS_MALLOC,
	SYS_FREE,
	ION_CMD_MAX,
};

const char* ion_cmd[] = {
	"ion_open",
	"ion_close",
	"ion_alloc",
	"ion_free",
	"ion_map",
	"ion_share",
	"ion_alloc_fd",
	"ion_import",
	"ion_sync_fd",
	"ion_get_phys",
	"mmap",
	"munmap",
	"share_close",
	"data_check",
	"data_read",
	"data_write",
	"print",
	"malloc",
	"free",
};

struct ion_param {
	int ion_fd;
	ion_user_handle_t handle;
	int share_fd;
	unsigned int share_id;
	unsigned char* map_ptr;
	size_t map_size;
	void *malloc_ptr;
};

int compare_cmd(const char* src, const char* dst, int len)
{
	char* p=NULL;
	int src_len = strlen(src);
	p = strchr(src, ',');
	if (p)
		src_len = p - src;
	return strncmp(src, dst, (src_len>len)?src_len:len);
}

int timeval_subtract(struct timeval* result, struct timeval* x, struct timeval* y)
{
	int nsec;

	if (x->tv_sec>y->tv_sec)
		return -1;

	if ((x->tv_sec==y->tv_sec) && (x->tv_usec>y->tv_usec))
		return -1;
	result->tv_sec = (y->tv_sec-x->tv_sec);
	result->tv_usec = (y->tv_usec-x->tv_usec);
	if (result->tv_usec<0)
	{
		result->tv_sec--;
		result->tv_usec+=1000000;
	}

	return 0;
}

int timeval_increase(struct timeval* x, struct timeval* y)
{
	int nsec;

	x->tv_sec += y->tv_sec;
	x->tv_usec += y->tv_usec;
	if (x->tv_usec>=1000000)
	{
		x->tv_sec++;
		x->tv_usec-=1000000;
	}

	return 0;
}

unsigned char* buff_test=NULL;
struct loop_cmd {
	char cmd[512];
	int count;
	long long used_tm;
	long long used_max;
	long long used_min;
};

int exec_ion_cmd(struct ion_param *param, struct loop_cmd *curr_cmd)//const char* line)
{
	int ret=0;
	int cmd;
	const char* line = curr_cmd->cmd;

	for(cmd=0; cmd<ION_CMD_MAX; cmd++) {
		if (!compare_cmd(line, ion_cmd[cmd], strlen(ion_cmd[cmd])))
			break;
	}

	// ´¦ÀíÃüÁî
	switch(cmd) {
	case ION_OPEN:
		ret = ion_open();
		if (ret>0) {
			param->ion_fd = ret;
		}
		if(verbose) printf("ion_open return %d\n", ret);
		break;
	case ION_CLOSE:
		if(verbose) printf("ion_close %d\n", param->ion_fd);
		ion_close(param->ion_fd);
		param->ion_fd = 0;
		break;
	case ION_ALLOC:
	{
		size_t a_len;
		size_t a_align;
		unsigned int flags;
		unsigned int a_heap_mask;
		if (sscanf(line, "ion_alloc,%d,%d,%d,%d", &a_len, &a_align, &a_heap_mask, &flags)==4){
			ret=ion_alloc(param->ion_fd, a_len, a_align, a_heap_mask, flags, &(param->handle));
			if(verbose) printf("alloc handle = %d\n", param->handle);
			if (ret)
				printf("!! alloc failed: %s\n", strerror(errno));
		} else 
			printf("get parameter failed\n");
		ret = 0;// don't exit
		if (buff_test==NULL) {
			int t_fd, t_share_fd;
			ion_user_handle_t t_handle;
			size_t t_len = 32*1024*1024;

			printf("alloc 32MB buffer for test\n");

			if (ion_alloc_fd(param->ion_fd, t_len, a_align, a_heap_mask, flags, &t_share_fd)) {
				printf("!! alloc failed: %s\n", strerror(errno));
				return -1;
			}

			buff_test = mmap(NULL, t_len, PROT_READ|PROT_WRITE,
				MAP_SHARED, t_share_fd, 0);
			if (buff_test==MAP_FAILED) {
				printf("!! mmap failed: %s\n", strerror(errno));
				return -1;
			}
			memset(buff_test, 0x5A, t_len);
		}

		break;
	}
	case ION_ALLOC_FD:
	{
		size_t a_len;
		size_t a_align;
		unsigned int flags;
		unsigned int a_heap_mask;
		if (sscanf(line, "ion_alloc_fd,%d,%d,%d,%d", &a_len, &a_align, &a_heap_mask, &flags)==4){
			ret=ion_alloc_fd(param->ion_fd, a_len, a_align, a_heap_mask, flags, &(param->share_fd));
			if(verbose) printf("alloc share fd = %d\n", param->share_fd);
			if (ret)
				printf("!! alloc failed: %s\n", strerror(errno));
		} else 
			printf("get parameter failed\n");
		ret = 0;// don't exit
		break;
	}
	case ION_FREE:
		if(verbose) printf("free handle = %d\n", param->handle);
		ion_free(param->ion_fd, param->handle);
		break;
	case ION_MAP:
	{
		size_t a_len;
		size_t a_offset;
		if (sscanf(line, "ion_map,%d,%d", &a_len, &a_offset)==2){
			param->map_size = a_len;
			ret=ion_map(param->ion_fd, param->handle, a_len, 
				PROT_READ|PROT_WRITE, MAP_SHARED, a_offset, &param->map_ptr,
				&(param->share_fd));
			if (ret)
				printf("!! ion_map failed: %s\n", strerror(errno));
			if(verbose) printf("ion_map: ptr=%p share_fd=%d\n", param->map_ptr, param->share_fd);
		} else 
			printf("get parameter failed\n");
		break;
	}
	case ION_SHARE:
		ret=ion_share(param->ion_fd, param->handle, &(param->share_fd));
		if(verbose) printf("get share fd=%d\n", param->share_fd);
		if (ret)
			printf("!! share failed %s\n", strerror(errno));
		break;
	case ION_IMPORT:
		ret = ion_import(param->ion_fd, param->share_fd, &(param->handle));
		if(verbose) printf("get handle=%d\n", param->handle);
		if (ret<0)
			printf("!! import failed %s\n", strerror(errno));
		break;
	case ION_GET_PHYS:
	{
		unsigned long phys;
		ret = ion_get_phys(param->ion_fd, param->handle, &phys);
		if (ret<0)
			printf("!! get phys failed %s\n", strerror(errno));
		if(verbose) printf("PHYS=0x%08lX\n", phys);
		break;
	}
	case SYS_MMAP:
	{
		size_t a_len;
		size_t a_offset;
		if (sscanf(line, "mmap,%d,%d", &a_len, &a_offset)==2){
			param->map_ptr = mmap(NULL, a_len, PROT_READ|PROT_WRITE,
				MAP_SHARED,	param->share_fd, a_offset);
			if (param->map_ptr==MAP_FAILED) {
				printf("!! mmap failed: %s\n", strerror(errno));
				ret = -1;
			}
			if(verbose) printf("mmap: ptr=%p\n", param->map_ptr);
		} else
			printf("get parameter failed\n");
		break;
	}
	case SYS_MUNMAP:
	{
		size_t a_len;
		munmap(param->map_ptr, param->map_size);
		break;
	}
	case SHARE_CLOSE:
		if(verbose) printf("close share fd=%d\n", param->share_fd);
		if (param->share_fd>0)
			close(param->share_fd);
		break;
	case DATA_CHECK:
	{
		size_t a_len;
		int i;
		int t;
		struct timeval start,stop,diff;
		if (sscanf(line, "data_check,%d", &a_len)==1){
			gettimeofday(&start,0);
			for (i=0; i<a_len; i++) {
				param->map_ptr[i] = (unsigned char)i;
			}
			for (i=0; i<a_len; i++)
				if (param->map_ptr[i] != (unsigned char)i) {
					printf("failed wrote %d read %d from mapped "
						   "memory\n", i, param->map_ptr[i]);
					break;
				}
		} else
			printf("get parameter failed\n");
		break;
	}
	case DATA_READ:
	{
		size_t a_len;
		int i;
		if (sscanf(line, "data_read,%d", &a_len)==1){
			memcpy(buff_test, param->map_ptr, a_len);
		}
		break;
	}
	case DATA_WRITE:
	{
		size_t a_len;
		int i;
		if (sscanf(line, "data_write,%d", &a_len)==1){
			memcpy(param->map_ptr, buff_test, a_len);
		}
		break;
	}
	case SYS_MALLOC:
	{
		size_t a_len;
		printf("malloc prev %p\n", param->malloc_ptr);
		if (sscanf(line, "malloc,%d", &a_len)==1){
			param->malloc_ptr = malloc(a_len);
			printf("malloc %p\n", param->malloc_ptr);
			memset(param->malloc_ptr, 1, a_len);
		}
		break;
	}
	case SYS_FREE:
		free(param->malloc_ptr);
		param->malloc_ptr = NULL;
		break;
	case ION_SYNC_FD:
		ion_sync_fd(param->ion_fd, param->share_fd);
		break;
	case PRINT:
		line += strlen("print,");
		printf("%s\n", line);
		break;
	}

	return ret;
}

int script_test(const char* fname)
{
	FILE* fp = fopen(fname, "r");
	int loop_count = 0;
	struct ion_param* loop_param = NULL;
	struct ion_param this_param;
	int collect_cmd = 0;
	struct loop_cmd loop_cmds[100];
	struct loop_cmd this_cmd;
	struct loop_cmd *curr_cmd;
	struct ion_param* curr_loop_param = NULL;
	int cmd_ind = 0;

	printf("read cmd from script file: %s\n", fname);
	
	if (fp==NULL)
	{
		perror("fopen");
		return -1;
	}

	while(1)
	{
		int i;
		char* line = NULL;
		if (loop_count>0 && collect_cmd==0)
			curr_cmd = &loop_cmds[cmd_ind++];
		else if (fgets(this_cmd.cmd, 511, fp)) {
			this_cmd.count = 0;
			this_cmd.used_tm = 0;
			this_cmd.used_max = 0;
			this_cmd.used_min = 0;
			curr_cmd = &this_cmd;
		} else
			break;
		line = curr_cmd->cmd;

		i=0;
		while(line[i]) {
			if (line[i]=='\r' || line[i]=='\n') line[i] = '\0';
			++i;
		}

		if (line[0]=='#' || line[0]=='\r' || line[0]=='\n')
			continue;

		if(verbose) printf("CMD: %s\n", line);

		if (!strncmp(line, "loop,", strlen("loop,"))) {
			sscanf(line, "loop,%d", &loop_count);
			if(verbose) printf("loop count: %d\n", loop_count);
			loop_param = malloc(sizeof(struct ion_param)*loop_count);
			memset(loop_param, 0, sizeof(struct ion_param)*loop_count);
			curr_loop_param = loop_param;
			collect_cmd = 1;
			for (i=0;i<100;i++) {
				loop_cmds[i].cmd[0] = 0;
				loop_cmds[i].used_tm = 0;
				loop_cmds[i].used_max = 0;
				loop_cmds[i].used_min = 0;
				loop_cmds[i].count = 0;
			}
			cmd_ind = 0;
			continue;
		}

		if (collect_cmd){
			strcpy(loop_cmds[cmd_ind].cmd, line);
			curr_cmd = &loop_cmds[cmd_ind];
			line = curr_cmd->cmd;
			++cmd_ind;
		}

		if (!strcmp(line, "end")) {
			collect_cmd = 0;
			--loop_count;
			cmd_ind = 0;
			++curr_loop_param;
			if(verbose) printf("----- loop: %d -----\n", loop_count);
			if (loop_count==0) {
				for(i=0; i<100 && loop_cmds[i].count>0; i++) {
					long long av = (loop_cmds[i].used_tm-loop_cmds[i].used_max-loop_cmds[i].used_min) / (loop_cmds[i].count-2);
					printf("CMD(%32.32s) : %.4d : %.8lld : AV(%lld)\n", loop_cmds[i].cmd,
						loop_cmds[i].count, loop_cmds[i].used_tm, av);
				}
				free(loop_param);
				loop_param = NULL;
				for (i=0;i<100;i++) {
					loop_cmds[i].cmd[0] = 0;
					loop_cmds[i].used_tm = 0;
					loop_cmds[i].used_max = 0;
					loop_cmds[i].used_min = 0;
					loop_cmds[i].count = 0;
				}
			}
		} else if (!strncmp(line, "pause", strlen("pause"))) {
			getc(stdin);
		} else {
			struct ion_param* param = loop_count?curr_loop_param:&this_param;
			struct timeval start,stop,diff;
			if (loop_count && param->ion_fd==0)
				memcpy(param, &this_param, sizeof(struct ion_param));

			gettimeofday(&start,0);
			if (exec_ion_cmd(param, curr_cmd)<0){
				printf("cmd exec fault, press Enter for exit!\n");
				getc(stdin);
				break;
			}
			gettimeofday(&stop,0);
			timeval_subtract(&diff,&start,&stop);
			long long tm = (long long)diff.tv_sec*1000*1000+diff.tv_usec;
			curr_cmd->used_tm += tm;
			if (!curr_cmd->used_max || tm > curr_cmd->used_max)
				curr_cmd->used_max = tm;
			else if (!curr_cmd->used_min || tm<curr_cmd->used_min)
				curr_cmd->used_min = tm;
			++curr_cmd->count;
			ALOGD("%s : %ld.%06ld\n", line, diff.tv_sec,diff.tv_usec);
		}
	}

	fclose(fp);

	free(loop_param);
	loop_param = NULL;

	return 0;
}

int main(int argc, char* argv[]) {
	int c;
	enum tests {
		ALLOC_TEST = 0, MAP_TEST, SHARE_TEST,
	};

	while (1) {
		static struct option opts[] = {
			{"alloc", no_argument, 0, 'a'},
			{"alloc_flags", required_argument, 0, 'f'},
			{"heap_mask", required_argument, 0, 'h'},
			{"map", no_argument, 0, 'm'},
			{"share", no_argument, 0, 's'},
			{"len", required_argument, 0, 'l'},
			{"align", required_argument, 0, 'g'},
			{"map_flags", required_argument, 0, 'z'},
			{"prot", required_argument, 0, 'p'},
			{"test", required_argument, 0, 't'},
			{"verbose", required_argument, 0, 'v'},
		};
		int i = 0;
		c = getopt_long(argc, argv, "af:h:l:mr:st:v", opts, &i);
		if (c == -1)
			break;

		switch (c) {
		case 'l':
			len = atol(optarg);
			break;
		case 'g':
			align = atol(optarg);
			break;
		case 'z':
			map_flags = 0;
			map_flags |= strstr(optarg, "PROT_EXEC") ?
				PROT_EXEC : 0;
			map_flags |= strstr(optarg, "PROT_READ") ?
				PROT_READ: 0;
			map_flags |= strstr(optarg, "PROT_WRITE") ?
				PROT_WRITE: 0;
			map_flags |= strstr(optarg, "PROT_NONE") ?
				PROT_NONE: 0;
			break;
		case 'p':
			prot = 0;
			prot |= strstr(optarg, "MAP_PRIVATE") ?
				MAP_PRIVATE	 : 0;
			prot |= strstr(optarg, "MAP_SHARED") ?
				MAP_PRIVATE	 : 0;
			break;
		case 'f':
			alloc_flags = atol(optarg);
			break;
		case 'h':
			heap_mask = atol(optarg);
			break;
		case 'a':
			test = ALLOC_TEST;
			break;
		case 'm':
			test = MAP_TEST;
			break;
		case 's':
			test = SHARE_TEST;
			break;
		case 't':
			script_test(optarg);
			return 0;
		case 'v':
			verbose = 1;
			break;
		}
	}
	printf("test %d, len %u, align %u, map_flags %d, prot %d, heap_mask %d,"
	       " alloc_flags %d\n", test, len, align, map_flags, prot,
	       heap_mask, alloc_flags);
	switch (test) {
		case ALLOC_TEST:
			ion_alloc_test();
			break;
		case MAP_TEST:
			ion_map_test();
			break;
		case SHARE_TEST:
			ion_share_test();
			break;
		default:
			printf("must specify a test (alloc, map, share)\n");
	}
	return 0;
}
