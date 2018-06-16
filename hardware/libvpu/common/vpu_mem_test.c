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

#include <cutils/log.h>
#include <include/vpu_mem.h>

#include "libion_vpu/ion_priv_vpu.h"
#include "vpu_mem_pool/vpu_mem_pool.h"
 
void vpu_mem_share_test()
{
 
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
        VPUMemLinear_t vpumem;
        VPUMemLinear_t lnkmem;
        VPUMemLinear_t cpymem;
        /* parent */

        VPUMallocLinear(&vpumem, 1920*1088*3/2);

        ALOGE("parent: phy %08x, vir %08x\n", vpumem.phy_addr, vpumem.vir_addr);

        strcpy(vpumem.vir_addr, "master");

        VPUMemDuplicate(&lnkmem, &vpumem);

        VPUMemLink(&lnkmem);

        ALOGE("lnkmem: phy %08x, vir %08x\n", lnkmem.phy_addr, lnkmem.vir_addr);

        strcpy(lnkmem.vir_addr, "lnkmem");

        VPUMemDuplicate(&cpymem, &lnkmem);
        
        ALOGE("parent: fd = %d\n", cpymem.offset);
        cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		*(int *)CMSG_DATA(cmsg) = cpymem.offset;
		/* send the fd */
		sendmsg(sd[0], &msg, 0);
		
        sleep(5);

        VPUFreeLinear(&vpumem);
        VPUFreeLinear(&lnkmem);

        VPUMemLink(&cpymem);
        VPUFreeLinear(&cpymem);

        ALOGE("parent quit\n");
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
 

		ALOGE("child %d\n", recv_fd); 
		ptr = mmap(NULL, 1920*1088*3/2, PROT_READ | PROT_WRITE, MAP_SHARED, recv_fd, 0);
		if (ptr == MAP_FAILED) {
			return;
		}
 
		ALOGE("child? [%10s] should be [master]\n", ptr);

        sleep(4);
	}
}


int vpu_mem_alloc_test()
{
    ALOGE("%s in\n", __func__);
    do {
        int i = 0;

        int ion_client;
        ion_client = ion_open();

        while (i++ < 500) {
 
            VPUMemLinear_t vpumem;
            VPUMemLinear_t cpymem;
            VPUMemLinear_t lnkmem;
            void *obj;
            /* parent */

            ALOGE("count %d\n", i);

            VPUMallocLinear(&vpumem, 1920*1088*3/2);

            ALOGE("parent: phy %08x, vir %08x\n", vpumem.phy_addr, vpumem.vir_addr);
            strcpy(vpumem.vir_addr, "master");

            VPUMemDuplicate(&cpymem, &vpumem);

            //ion_get_share(ion_client, cpymem.offset, &obj);

            VPUMemLink(&cpymem);

            VPUMemDuplicate(&lnkmem, &cpymem);
            VPUMemLink(&lnkmem);

            ALOGE("copy: phy %08x, vir %08x\n", cpymem.phy_addr, cpymem.vir_addr);

            VPUFreeLinear(&cpymem);
            VPUFreeLinear(&vpumem);
            VPUFreeLinear(&lnkmem); 
        }

        return 0;
    } while (0);

    return -1;
}

int vpu_mem_from_fd_test()
{
    ALOGE("%s in\n", __func__);
    do {
        int i;
        int ion_client = ion_open();
        int share_fd[20];
        VPUMemLinear_t vpumem, lnkmem;
        VPUMemLinear_t normem, cpymem, fnlmem;
        int cnt = 5;
        int len = 0x100000;

         vpu_display_mem_pool *pool = open_vpu_memory_pool(); 
        while (cnt-- > 0) { 
            VPUMallocLinear(&normem, len);
            VPUMemDuplicate(&cpymem, &normem);
            VPUMemLink(&cpymem);
            VPUMemDuplicate(&fnlmem, &cpymem);
            VPUMemLink(&fnlmem);
            VPUFreeLinear(&cpymem);
            VPUFreeLinear(&fnlmem);
            VPUFreeLinear(&normem); 
        }
        close_vpu_memory_pool(pool);
        ion_close(ion_client);

        return 0;
    }
    while (0);

    return -1;
}

int prot = PROT_READ | PROT_WRITE;
int map_flags = MAP_SHARED;
int alloc_flags = 0;
size_t len = 1024*1024, align = 0;
int heap_mask = 2;

void ion_mytest1()
{
	int fd, map_fd, ret;
	size_t i;
	struct ion_handle *handle;
    struct ion_handle *handle2;
	unsigned long phys = 0;
	unsigned char *ptr;
	int count = 0;
    int share_fd;

	fd = ion_open();
	if (fd < 0)
		return;

    ret = ion_alloc(fd, len, align, heap_mask, alloc_flags, &handle);

    if (ret){
        printf("%s failed: %s\n", __func__, strerror(ret));
        return;
    }
    while(1) {
        unsigned char *ptrx;
        int map_fd;
    	printf("%s: TEST %d\n", __func__, count++);
        ion_share(fd, handle, &share_fd);
        ion_import(fd, share_fd, &handle2);
        ptrx = mmap(NULL, len, prot, map_flags, share_fd, 0);
    	if (ret)
    		return;
 
    	ion_get_phys(fd, handle2, &phys);
    	if(ret)
    		return;
    	printf("PHYS=%X\n", phys);

    	/* clean up properly */
    	munmap(ptrx, len);
        ret = ion_free(fd, handle2);
        close(share_fd);
    }

    ret = ion_free(fd, handle);
	ion_close(fd);
}

int main(int argc, char **argv)
{
    //ion_mytest1();
     vpu_mem_alloc_test();
    //vpu_mem_from_fd_test();

    return 0;
}
