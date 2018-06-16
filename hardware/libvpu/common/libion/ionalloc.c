#include <stdarg.h>
#include "ion_priv.h"
#include "log.h"

inline size_t roundUpToPageSize(size_t x) {
        return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
}

static int validate(private_handle_t *h) 
{
        const private_handle_t *hnd = (const private_handle_t *)h;
    
        if( !hnd ||
                hnd->s_num_ints != NUM_INTS || 
                hnd->s_num_fds != NUM_FDS ||
                hnd->s_magic != MAGIC) {
                ALOGE("Invalid ion handle (at %p)", hnd);
                return -EINVAL;
        }
        return 0;
}
unsigned long ion_get_flags(enum ion_module_id id, enum _ion_heap_type type)
{
        unsigned long flags = 0;

	switch (id) {
	case ION_MODULE_CAM:
		flags |= 1 << ION_CAM_ID;
		break;
	case ION_MODULE_VPU:
		flags |= 1 << ION_VPU_ID;
		break;
	case ION_MODULE_UI:
		flags |= 1 << ION_UI_ID;
		break;
	default:
		break;
	}

	switch (type) {
	case _ION_HEAP_RESERVE:
		flags |= 1 << ION_NOR_HEAP_ID;
		break;
	default:
		flags |= 1 << ION_NOR_HEAP_ID;
		break;
	}
        return flags;
}
static int ion_alloc(struct ion_device_t *ion, unsigned long size, enum _ion_heap_type type, ion_buffer_t **data)
{
        int err = 0;
        struct ion_handle_data handle_data;
        struct ion_fd_data fd_data;
        struct ion_allocation_data ionData;
        struct ion_phys_data phys_data;
        void *virt = 0;
	unsigned long phys = 0;

        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }
        private_handle_t *hnd = (private_handle_t *)malloc(sizeof(private_handle_t));
        if(!hnd){
                ALOGE("%s: Failed to malloc private_handle_t hnd", __FUNCTION__);
                return -EINVAL;
        }

        ionData.len = roundUpToPageSize(size);
        ionData.align = dev->align;
        if(dev->ionfd == FD_INIT) {
                dev->ionfd = open(ION_DEVICE, O_RDONLY);
                if(dev->ionfd < 0){
                        ALOGE("%s: Failed to open %s - %s",
                                        __FUNCTION__, ION_DEVICE, strerror(errno));
                        goto free_hnd;
                }
        }
	
        ionData.flags = ion_get_flags(dev->id, type);
        err = ioctl(dev->ionfd, ION_IOC_ALLOC, &ionData);
        if(err) {
                ALOGE("%s: ION_IOC_ALLOC failed to alloc 0x%lx bytes with error(flags = 0x%x) - %s", 
                                __FUNCTION__, size, ionData.flags, strerror(errno));
                goto err_alloc;
        }
        fd_data.handle = ionData.handle;
        handle_data.handle = ionData.handle;

        err = ioctl(dev->ionfd, ION_IOC_MAP, &fd_data);
        if(err) {
                ALOGE("%s: ION_IOC_MAP failed with error - %s",
                                __FUNCTION__, strerror(errno));
                goto err_map;
        }

        phys_data.handle = ionData.handle;
        err = ioctl(dev->ionfd, ION_CUSTOM_GET_PHYS, &phys_data);
        if(err) {
                ALOGE("%s: ION_CUSTOM_GET_PHYS failed with error - %s",
                                __FUNCTION__, strerror(errno));
                goto err_phys;
        }

        virt = mmap(0, ionData.len, PROT_READ|PROT_WRITE,
                        MAP_SHARED, fd_data.fd, 0);
        if(virt == MAP_FAILED) {
                ALOGE("%s: Failed to map the allocated memory: %s",
                                __FUNCTION__, strerror(errno));
                goto err_mmap;
        }

        err = ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
        if(err){
                ALOGE("%s: ION_IOC_FREE failed with error - %s",
                                __FUNCTION__, strerror(errno));
                goto err_free;
        }

        hnd->data.virt = virt;
        hnd->data.phys = phys_data.phys;
        hnd->data.size = ionData.len; 

        hnd->fd = fd_data.fd;
        hnd->pid = getpid();
        hnd->handle = fd_data.handle;
        hnd->s_num_ints = NUM_INTS;
        hnd->s_num_fds = NUM_FDS;
        hnd->s_magic = MAGIC;

        *data = &hnd->data;
        ALOGV("%s: tid = %d, base %p, phys %lx, size %luK, fd %d, handle %p", 
                        __FUNCTION__, pthread_self(), hnd->data.virt, hnd->data.phys, hnd->data.size/1024, hnd->fd, hnd->handle);
        return 0;

err_free:
        munmap(virt, size);
err_mmap:
err_phys:
        close(fd_data.fd);
err_map:
        ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
err_alloc:
        close(dev->ionfd);
free_hnd:
        free(hnd);
        dev->ionfd = FD_INIT;
        *data = NULL;
        err = -errno;
        return err;

}
static int ion_free(struct ion_device_t *ion, ion_buffer_t *data)
{
        int err = 0;

        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }
        private_handle_t *hnd = (private_handle_t *)data;
        if(validate(hnd) < 0)
                return -EINVAL;

        ALOGV("%s: tid %d, base %p, phys %lx, size %luK, fd %d, handle %p", 
                        __FUNCTION__, pthread_self(),hnd->data.virt, hnd->data.phys, hnd->data.size/1024, hnd->fd, hnd->handle);
        if(!hnd->data.virt) {
                ALOGE("%s: Invalid free", __FUNCTION__);
                return -EINVAL;
        }
        err = munmap(hnd->data.virt, hnd->data.size);
        if(err)
                ALOGE("%s: munmap failed", __FUNCTION__);

        close(hnd->fd);
        free(hnd);
        hnd = NULL;
        return err;
}

static int ion_share(struct ion_device_t *ion, ion_buffer_t *data, int *share_fd)
{
        int err = 0;
        struct ion_fd_data fd_data;

        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }
        private_handle_t *hnd = (private_handle_t *)data;
        if(validate(hnd) < 0)
                return -EINVAL;

        fd_data.handle = hnd->handle;
        err = ioctl(dev->ionfd, ION_IOC_SHARE, &fd_data);
        if(err) {
                ALOGE("%s: ION_IOC_SHARE failed with error - %s",
                                __FUNCTION__, strerror(errno));
                *share_fd = FD_INIT;
                err = -errno;
        }else{
                *share_fd = fd_data.fd;
        }
        ALOGV("%s: tid = %d, base %p, phys %lx, size %luK, fd %d, handle: %p", 
                        __FUNCTION__, pthread_self(), hnd->data.virt, hnd->data.phys, hnd->data.size/1024, *share_fd, hnd->handle);
        return err;
}

static int ion_map(struct ion_device_t *ion, int share_fd, ion_buffer_t **data)
{
        int err = 0;
        void *virt = NULL;
        unsigned long phys = 0;
	struct ion_phys_data phys_data;
	struct ion_fd_data fd_data;
	struct ion_handle_data handle_data;

        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }
        private_handle_t *hnd = (private_handle_t *)malloc(sizeof(private_handle_t));
        if(!hnd){
                ALOGE("%s: Failed to malloc private_handle_t hnd", __FUNCTION__);
                return -EINVAL;
        }

        if(dev->ionfd == FD_INIT) {
                dev->ionfd = open(ION_DEVICE, O_RDONLY);
                if(dev->ionfd < 0){
                        ALOGE("%s: Failed to open %s - %s",
                                        __FUNCTION__, ION_DEVICE, strerror(errno));
                        goto free_hnd;
                }
        }

        fd_data.fd = share_fd;
        err = ioctl(dev->ionfd, ION_IOC_IMPORT, &fd_data);
	if(err){
                ALOGE("%s: ION_IOC_IMPORT failed with error - %s",
                                __FUNCTION__, strerror(errno));
                goto err_import;
	}
        phys_data.handle = fd_data.handle;
        err = ioctl(dev->ionfd, ION_CUSTOM_GET_PHYS, &phys_data);
        if(err) {
                ALOGE("%s: ION_CUSTOM_GET_PHYS failed with error - %s",
                                __FUNCTION__, strerror(errno));
                goto err_phys;
        }
        handle_data.handle = fd_data.handle;
        virt = mmap(0, phys_data.size, PROT_READ| PROT_WRITE, MAP_SHARED, share_fd, 0);
        if(virt == MAP_FAILED) {
                ALOGE("%s: Failed to map memory in the client: %s",
                                __FUNCTION__, strerror(errno));
                goto err_mmap;
        }
        err = ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
        if(err){
                ALOGE("%s: ION_IOC_FREE failed with error - %s",
                                __FUNCTION__, strerror(errno));
		goto err_free;
	}

        hnd->data.virt = virt;
        hnd->data.phys = phys_data.phys;
        hnd->data.size = phys_data.size;

        hnd->fd = share_fd;
        hnd->pid = getpid();
        hnd->handle = fd_data.handle;
        hnd->s_num_ints = NUM_INTS;
        hnd->s_num_fds = NUM_FDS;
        hnd->s_magic = MAGIC;

        *data = &hnd->data;
        ALOGV("%s: tid = %d, base %p, phys %lx, size %luK, fd %d, handle %p", 
                        __FUNCTION__, pthread_self(), hnd->data.virt, hnd->data.phys, hnd->data.size/1024, hnd->fd, hnd->handle);

        return 0;
err_free:
err_mmap:
err_phys:
        ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
err_import:
        close(dev->ionfd);
free_hnd:
        free(hnd);
        dev->ionfd = FD_INIT;
        *data = NULL;
	err = -errno;
        return err;
}
static int ion_unmap(struct ion_device_t *ion, ion_buffer_t *data)
{
        int err = 0;

        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }
        private_handle_t *hnd = (private_handle_t *)data;
        if(validate(hnd) < 0)
                return -EINVAL;

        ALOGV("%s: base %p, phys %lx, size %luK, fd %d, handle %p", 
                        __FUNCTION__, hnd->data.virt, hnd->data.phys, hnd->data.size/1024, hnd->fd, hnd->handle);
	if(!hnd->data.virt) {
                ALOGE("%s: Invalid free", __FUNCTION__);
                return -EINVAL;
        }
        err = munmap(hnd->data.virt, hnd->data.size);
        if(err)
                return err;

        close(hnd->fd);
        free(hnd);
        hnd = NULL;
        return 0;
}

static int ion_cache_op(struct ion_device_t *ion, ion_buffer_t *data, enum cache_op_type type)
{
        int err = 0;
	struct ion_cacheop_data cache_data;
        unsigned int cmd;

        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }

        private_handle_t *hnd = (private_handle_t *)data;
        if(validate(hnd) < 0)
                return -EINVAL;

        switch(type) {
        case ION_CLEAN_CACHE:
                cache_data.type = ION_CACHE_CLEAN;
                break;
        case ION_INVALID_CACHE:
                cache_data.type = ION_CACHE_INV;
                break;
        case ION_FLUSH_CACHE:
                cache_data.type = ION_CACHE_FLUSH;
                break;
        default:
	        ALOGE("%s: cache_op_type not support - %s",
                    __FUNCTION__, strerror(errno));
            return -EINVAL;
        }
	cache_data.handle = hnd->handle;
	cache_data.virt = hnd->data.virt;
	err = ioctl(dev->ionfd, ION_CUSTOM_CACHE_OP, &cache_data);
#if 1
        if(err) {
                struct ion_pmem_region region;

                region.offset = (unsigned long)hnd->data.virt;
                region.len = hnd->data.size;
                err = ioctl(hnd->fd, ION_PMEM_CACHE_FLUSH, &region);
        }
#endif
	if(err){
		ALOGE("%s: ION_CUSTOM_CACHE_OP failed with error - %s",
                __FUNCTION__, strerror(errno));
                err = -errno;
		return err;
	}
	return 0;
}

int ion_perform(struct ion_device_t *ion, int operation, ... )
{
        int err = 0;
        va_list args;
        private_device_t *dev = (private_device_t *)ion;
        if(!dev){
                ALOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
                return -EINVAL;
        }

        va_start(args, operation);
        switch(operation) {
        case ION_MODULE_PERFORM_QUERY_BUFCOUNT: 
        case ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED: 
        {
                struct ion_client_info info;   
                unsigned long i, _count = 0, size = 0, *p;
        
                if(operation == ION_MODULE_PERFORM_QUERY_BUFCOUNT)
                        size = va_arg(args, unsigned long); 
                p = (unsigned long *)va_arg(args, void *);

                memset(&info, 0, sizeof(struct ion_client_info));
	        err = ioctl(dev->ionfd, ION_CUSTOM_GET_CLIENT_INFO, &info);
                if(err) {
		        ALOGE("%s: ION_GET_CLIENT failed with error - %s",
                                __FUNCTION__, strerror(errno));
                        err = -errno;
	        } else {
                        if(operation == ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED)
                                *p = info.total_size;
                        else {
                                for(i = 0; i < info.count; i++) {
                                        if(info.buf[i].size == size)
                                                _count++;
                                }
                                *p = _count;
                        }
                        err = 0;
                }
                break;
        }
        
        case ION_MODULE_PERFORM_QUERY_HEAP_SIZE: 
        case ION_MODULE_PERFORM_QUERY_HEAP_ALLOCATED: 
        {
                struct ion_heap_info info; 
                
                unsigned long *p = (unsigned long *)va_arg(args, void *); 

                info.id = ION_NOR_HEAP_ID;

	        err = ioctl(dev->ionfd, ION_CUSTOM_GET_HEAP_INFO, &info);
                if(err) {
		        ALOGE("%s: ION_GET_CLIENT failed with error - %s",
                                __FUNCTION__, strerror(errno));
                        err = -errno;
	        } else {
                        if(operation == ION_MODULE_PERFORM_QUERY_HEAP_SIZE)
                                *p = info.total_size;
                        else
                                *p = info.allocated_size;
                        err = 0;
                }
                break;
        }
        default:
                ALOGE("%s: operation(0x%x) not support", __FUNCTION__, operation);
                err = -EINVAL;
                break;
        }

        va_end(args);
        return err;
}
int ion_open(unsigned long align, enum ion_module_id id, ion_device_t **ion)
{
        char name[16];
        private_device_t *dev = (private_device_t *)malloc(sizeof(private_device_t));
        if(!dev)
                return -EINVAL;

        dev->ionfd = FD_INIT;
        dev->align = align;
        dev->id = id;
        dev->ion.alloc = ion_alloc;
        dev->ion.free = ion_free;
        dev->ion.share = ion_share;
        dev->ion.map = ion_map;
        dev->ion.unmap = ion_unmap;
        dev->ion.cache_op = ion_cache_op;
        dev->ion.perform = ion_perform;

        *ion = &dev->ion;
        switch (id) {
        case ION_MODULE_VPU:
                strcpy(name, "vpu");
                break;
        case ION_MODULE_CAM:
                strcpy(name, "camera");
                break;
        case ION_MODULE_UI:
                strcpy(name, "ui");
                break;
        default:
                strcpy(name, "ui");
                break;
        }
        ALOGV("Ion(version: %s) is successfully opened by %s",
                        ION_VERSION, name);
        return 0;
}
int ion_close(ion_device_t *ion)
{
        private_device_t *dev = (private_device_t *)ion;

        if(dev->ionfd != FD_INIT)
                close(dev->ionfd);
        free(dev);

        return 0;
}
