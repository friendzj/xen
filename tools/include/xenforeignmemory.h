/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef XENFOREIGNMEMORY_H
#define XENFOREIGNMEMORY_H

/*
 * This library allows you to map foreign domain memory, subject to
 * permissions for both the process and the domain in which the
 * process runs.
 */

#include <stdint.h>
#include <stddef.h>

#include <xen/xen.h>

/* Callers who don't care don't need to #include <xentoollog.h> */
struct xentoollog_logger;

typedef struct xenforeignmemory_handle xenforeignmemory_handle;

/*
 * Return a handle onto the foreign memory mapping driver.  Logs errors.
 *
 * Note: After fork(2) a child process must not use any opened
 * foreignmemory handle inherited from their parent, nor access any
 * grant mapped areas associated with that handle.
 *
 * The child must open a new handle if they want to interact with
 * foreignmemory.
 *
 * Calling exec(2) in a child will safely (and reliably) reclaim any
 * resources which were allocated via a xenforeignmemory_handle in the
 * parent.
 *
 * A child which does not call exec(2) may safely call
 * xenforeignmemory_close() on a xenforeignmemory_handle inherited
 * from their parent. This will attempt to reclaim any resources
 * associated with that handle. Note that in some implementations this
 * reclamation may not be completely effective, in this case any
 * affected resources remain allocated.
 *
 * Calling xenforeignmemory_close() is the only safe operation on a
 * xenforeignmemory_handle which has been inherited.
 */
xenforeignmemory_handle *xenforeignmemory_open(struct xentoollog_logger *logger,
                                               unsigned open_flags);

/*
 * Close a handle previously allocated with xenforeignmemory_open().
 *
 * Under normal circumstances (i.e. not in the child after a fork)
 * xenforeignmemory_unmap() should be used on all mappings allocated
 * by xenforeignmemory_map() prior to closing the handle in order to
 * free up resources associated with those mappings.
 *
 * This is the only function which may be safely called on a
 * xenforeignmemory_handle in a child after a
 * fork. xenforeignmemory_unmap() must not be called under such
 * circumstances.
 */
int xenforeignmemory_close(xenforeignmemory_handle *fmem);

/*
 * Maps a range within one domain to a local address range.  Mappings
 * must be unmapped with xenforeignmemory_unmap and should follow the
 * same rules as mmap regarding page alignment.
 *
 * prot is as for mmap(2).
 *
 * @arr is an array of @pages gfns to be mapped linearly in the local
 * address range. @err is an (optional) output array used to report
 * per-page errors, as errno values.
 *
 * If @err is given (is non-NULL) then the mapping may partially
 * succeed and return a valid pointer while also using @err to
 * indicate the success (0) or failure (errno value) of the individual
 * pages. The global errno thread local variable is not valid in this
 * case.
 *
 * If @err is not given (is NULL) then on failure to map any page any
 * successful mappings will be undone and NULL will be returned. errno
 * will be set to correspond to the first failure (which may not be
 * the most critical).
 *
 * It is also possible to return NULL due to a complete failure,
 * i.e. failure to even attempt the mapping, in this case the global
 * errno will have been set and the contents of @err (if given) is
 * invalid.
 *
 * Note that it is also possible to return non-NULL with the contents
 * of @err indicating failure to map every page.
 */
void *xenforeignmemory_map(xenforeignmemory_handle *fmem, uint32_t dom,
                           int prot, size_t pages,
                           const xen_pfn_t arr[/*pages*/], int err[/*pages*/]);

/*
 * Almost like the previous one but also accepts two additional parameters:
 *
 * @addr is used as a hint address for foreign map placement (see mmap(2)).
 * @flags is a set of additional flags as for mmap(2). Not all of the flag
 * combinations are possible due to implementation details on different
 * platforms.
 */
void *xenforeignmemory_map2(xenforeignmemory_handle *fmem, uint32_t dom,
                            void *addr, int prot, int flags, size_t pages,
                            const xen_pfn_t arr[/*pages*/], int err[/*pages*/]);

/*
 * Unmap a mapping previous created with xenforeignmemory_map().
 *
 * Returns 0 on success on failure sets errno and returns -1.
 */
int xenforeignmemory_unmap(xenforeignmemory_handle *fmem,
                           void *addr, size_t pages);

/**
 * This function restricts the use of this handle to the specified
 * domain.
 *
 * @parm fmem handle to the open foreignmemory interface
 * @parm domid the domain id
 * @return 0 on success, -1 on failure.
 */
int xenforeignmemory_restrict(xenforeignmemory_handle *fmem,
                              domid_t domid);

typedef struct xenforeignmemory_resource_handle xenforeignmemory_resource_handle;

/**
 * This function maps a guest resource.
 *
 * @parm fmem handle to the open foreignmemory interface
 * @parm domid the domain id
 * @parm type the resource type
 * @parm id the type-specific resource identifier
 * @parm frame base frame index within the resource
 * @parm nr_frames number of frames to map
 * @parm paddr pointer to an address passed through to mmap(2)
 * @parm prot passed through to mmap(2)
 * @parm POSIX-only flags passed through to mmap(2)
 * @return pointer to foreignmemory resource handle on success, NULL on
 *         failure
 *
 * *paddr is used, on entry, as a hint address for foreign map placement
 * (see mmap(2)) so should be set to NULL if no specific placement is
 * required. On return *paddr contains the address where the resource is
 * mapped.
 * As for xenforeignmemory_map2() flags is a set of additional flags
 * for mmap(2). Not all of the flag combinations are possible due to
 * implementation details on different platforms.
 */
xenforeignmemory_resource_handle *xenforeignmemory_map_resource(
    xenforeignmemory_handle *fmem, domid_t domid, unsigned int type,
    unsigned int id, unsigned long frame, unsigned long nr_frames,
    void **paddr, int prot, int flags);

/**
 * This function releases a previously acquired resource.
 *
 * @parm fmem handle to the open foreignmemory interface
 * @parm fres handle to the acquired resource
 *
 * Returns 0 on success on failure sets errno and returns -1.
 */
int xenforeignmemory_unmap_resource(
    xenforeignmemory_handle *fmem, xenforeignmemory_resource_handle *fres);

/**
 * Determine the maximum size of a specific resource.
 *
 * @parm fmem handle to the open foreignmemory interface
 * @parm domid the domain id
 * @parm type the resource type
 * @parm id the type-specific resource identifier
 *
 * Return 0 on success and fills in *size, with a value in bytes.  Sets errno
 * and return -1 on error.
 */
int xenforeignmemory_resource_size(
    xenforeignmemory_handle *fmem, domid_t domid, unsigned int type,
    unsigned int id, size_t *size);

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
