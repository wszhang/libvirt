/*
 * lxctools_driver.c: core driver methods for managing LXCTool Containers
 *
 * Copyright (C) 2010-2015 Red Hat, Inc.
 * Copyright (C) 2006, 2007 Binary Karma
 * Copyright (C) 2006 Shuveb Hussain
 * Copyright (C) 2007 Anoop Joe Cyriac
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 * Shuveb Hussain <shuveb@binarykarma.com>
 * Anoop Joe Cyriac <anoop@binarykarma.com>
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <lxc/lxccontainer.h>

#include "virerror.h"
#include "datatypes.h"
#include "virbuffer.h"
#include "nodeinfo.h"
#include "viralloc.h"
#include "virfile.h"
#include "virtypedparam.h"
#include "virlog.h"
#include "vircommand.h"
#include "viruri.h"
#include "virstats.h"
#include "virstring.h"
#include "access/viraccessapicheck.h"
#include "lxctools_conf.h"

#include "lxctools_driver.h"

#define VIR_FROM_THIS VIR_FROM_LXCTOOLS

VIR_LOG_INIT("lxctools.lxctools_driver");
/* TODO:
 * - add better errors to DomainInfo
 *   - Debug DomainInfo (cont1 doesnt show up)
 *   redo start with proper api call (or at least try)
 * - write some tests
 * - migration
 * - create, delete
 * - XML impl
 * - debug
 */
/*
 * Src: Begin
 *      - Generate XML to pass to dst
 *      - Generate optional cookie to pass to dst
 */
static char *
lxctoolsDomainMigrateBegin3Params(virDomainPtr domain,
                                  virTypedParameterPtr params,
                                  int nparams,
                                  char **cookieout ATTRIBUTE_UNUSED,
                                  int *cookieoutlen ATTRIBUTE_UNUSED,
                                  unsigned int flags)
{
    virDomainObjPtr vm = NULL;
    struct lxctools_driver *driver = domain->conn->privateData;
    char *xml = NULL;

    virCheckFlags(0, NULL);
    if (virTypedParamsValidate(params, nparams, LXCTOOLS_MIGRATION_PARAMETERS) < 0)
        return NULL;

    vm = virDomainObjListFindByName(driver->domains, "cont1");

    if (!vm) {
        virReportError(VIR_ERR_NO_DOMAIN, "%s",
                       _("no domain with matching uuid"));
    }

    if (!virDomainObjIsActive(vm)) {
        virReportError(VIR_ERR_OPERATION_INVALID,
                       "%s", _("domain is not running"));
        goto cleanup;
    }

   // xml = virDomainDefFormat(vm->def, VIR_DOMAIN_DEF_FORMAT_SECURE);
 cleanup:
    if (vm)
        virObjectUnlock(vm);
    return xml;
}

/*
 * Dst: Prepare
 *      - Get ready to accept incoming VM
 *      - Generate optional cookie to pass to src
 */
static int
lxctoolsDomainMigratePrepare3Params(virConnectPtr dconn,
                                    virTypedParameterPtr params,
                                    int nparams,
                                    const char *cookiein ATTRIBUTE_UNUSED,
                                    int cookieinlen ATTRIBUTE_UNUSED,
                                    char **cookieout ATTRIBUTE_UNUSED,
                                    int *cookieoutlen ATTRIBUTE_UNUSED,
                                    char **uri_out ATTRIBUTE_UNUSED,
                                    unsigned int flags)
{
    struct lxctools_driver *driver = dconn->privateData;
    virDomainObjPtr vm = NULL;
    int ret = -1;
    virCheckFlags(0, -1);
    if (virTypedParamsValidate(params, nparams, LXCTOOLS_MIGRATION_PARAMETERS) < 0)
        goto cleanup;

    vm = virDomainObjListFindByName(driver->domains, "cont1");

    if (!vm) {
        virReportError(VIR_ERR_NO_DOMAIN, "%s",
                       _("no domain with matching uuid"));
        goto cleanup;
    }

    if (!virDomainObjIsActive(vm)) {
        virReportError(VIR_ERR_OPERATION_INVALID,
                       "%s", _("domain is not running"));
        goto cleanup;
    }


    ret = 0;
 cleanup:
    if(vm)
        virObjectUnlock(vm);
    return ret;
}

/*
 * Src: Perfom
 *      -Start migration and wait for send completion
 *      - Generate optional cookie to pass to dst
 */
static int
lxctoolsDomainMigratePerform3Params(virDomainPtr domain ATTRIBUTE_UNUSED,
                                    const char *dconnuri ATTRIBUTE_UNUSED,
                                    virTypedParameterPtr params ATTRIBUTE_UNUSED,
                                    int nparams ATTRIBUTE_UNUSED,
                                    const char* cookiein ATTRIBUTE_UNUSED,
                                    int cookieinlen ATTRIBUTE_UNUSED,
                                    char **cookieout ATTRIBUTE_UNUSED,
                                    int *cookieoutlen ATTRIBUTE_UNUSED,
                                    unsigned int flags)
{
    virCheckFlags(0, -1);
    return 0;
}

/*
 * Dst: Finish
 *      - Wait for recv completion and check status
 *      - Kill off VM if failed, resume if success
 *      - Generate optional cookie to pass to src
 */
static virDomainPtr
lxctoolsDomainMigrateFinish3Params(virConnectPtr dconn ATTRIBUTE_UNUSED,
                                   virTypedParameterPtr params ATTRIBUTE_UNUSED,
                                   int nparams ATTRIBUTE_UNUSED,
                                   const char* cookiein ATTRIBUTE_UNUSED,
                                   int cookieinlen ATTRIBUTE_UNUSED,
                                   char **cookieout ATTRIBUTE_UNUSED,
                                   int *cookieoutlen ATTRIBUTE_UNUSED,
                                   unsigned int flags,
                                   int cancelled ATTRIBUTE_UNUSED)
{
    virCheckFlags(0, NULL);
    return NULL;
}

/*
 * Src: Confirm
 *      - Kill off VM if success, resume if failed
 */
static int
lxctoolsDomainMigrateConfirm3Params(virDomainPtr domain ATTRIBUTE_UNUSED,
                                    virTypedParameterPtr params ATTRIBUTE_UNUSED,
                                    int nparams ATTRIBUTE_UNUSED,
                                    const char *cookiein ATTRIBUTE_UNUSED,
                                    int cookkieinlen ATTRIBUTE_UNUSED,
                                    unsigned int flags,
                                    int cancelled ATTRIBUTE_UNUSED)
{
    virCheckFlags(0, -1);
    return 0;
}

/*
 * Restore uses the xml parameter as domain name, because this
 * driver has no way to know which domain was saved.
 */
static int
lxctoolsDomainRestoreFlags(virConnectPtr conn, const char* from,
                           const char* dxml ATTRIBUTE_UNUSED,
                           unsigned int flags)
{
    struct lxctools_driver *driver = conn->privateData;
    virDomainObjPtr vm = NULL;
    struct lxc_container* cont;
    int ret = -1;
    char* cont_name = NULL;
    virCheckFlags(0, -1);

    if ((cont_name = getContainerNameFromPath(from)) == NULL) {
        virReportError(VIR_ERR_INVALID_ARG, "%s",
                       _("didn't find containername in path"));
        goto cleanup;
    }
printf("as:%s\n", cont_name);
    vm = virDomainObjListFindByName(driver->domains,
                                    cont_name);

    if (!vm) {
        virReportError(VIR_ERR_NO_DOMAIN,
                        _("no domain with name '%s'"), cont_name);
        goto cleanup;
    }

    if (!(cont = vm->privateData)) {
        virReportError(VIR_ERR_INTERNAL_ERROR,
                       _("inconsistent data for container '%s'"),
                       cont_name);
        goto cleanup;
    }

    if (!virFileExists(from)) {
        virReportError(VIR_ERR_INVALID_ARG,
                       _("path '%s' does not exist"),
                       from);
        goto cleanup;
    }

    if (!virFileIsDir(from)) {
        virReportError(VIR_ERR_INVALID_ARG,
                       _("path '%s' is not a directory"),
                       from);
        goto cleanup;
    }

    if (cont->is_running(cont)) {
        virReportError(VIR_ERR_OPERATION_DENIED, "%s",
                       _("domain is in running state"));
        goto cleanup;
    }

    if(!cont->may_control(cont)) {
	virReportError(VIR_ERR_OPERATION_DENIED, "%s",
		       _("domain may not be controlled"));
	goto cleanup;
    }

    if(!criuExists()) {
        virReportError(VIR_ERR_OPERATION_DENIED, "%s",
                       _("criu binary not found in PATH"));
        goto cleanup;
    }

    if(!cont->restore(cont, (char*)from, false)) {
            virReportError(VIR_ERR_OPERATION_FAILED, "%s",
                           _("lxc api call failed. check lxc log for more information"));
            goto cleanup;
    }
    vm->def->id = cont->init_pid(cont);
    virDomainObjSetState(vm, VIR_DOMAIN_RUNNING, VIR_DOMAIN_RUNNING_RESTORED);
    ret = 0;

 cleanup:
    if (vm)
        virObjectUnlock(vm);
    VIR_FREE(cont_name);
    return ret;
}

static int
lxctoolsDomainRestore(virConnectPtr conn, const char* from)
{
    return lxctoolsDomainRestoreFlags(conn, from, NULL, 0);
}

static int
lxctoolsDomainSaveFlags(virDomainPtr domain, const char* to,
                        const char* dxml ATTRIBUTE_UNUSED,
                        unsigned int flags)
{
    struct lxctools_driver *driver = domain->conn->privateData;
    virDomainObjPtr vm = NULL;
    struct lxc_container* cont;
    int ret = -1;
    char* save_path = NULL;
    virCheckFlags(VIR_DOMAIN_SAVE_RUNNING | VIR_DOMAIN_SAVE_PAUSED, -1);

    vm = virDomainObjListFindByUUID(driver->domains, domain->uuid);

    if(!vm) {
        virReportError(VIR_ERR_NO_DOMAIN,
                        _("no domain with name '%s'"), domain->name);
        goto cleanup;
    }

    cont = vm->privateData;

    if (!virFileExists(to)) {
        virReportError(VIR_ERR_INVALID_ARG,
                       _("path '%s' does not exist"),
                       to);
        goto cleanup;
    }

    if (!virFileIsDir(to)) {
        virReportError(VIR_ERR_INVALID_ARG,
                       _("path '%s' is not a directory"),
                       to);
        goto cleanup;
    }

    if (!cont->is_running(cont)) {
        virReportError(VIR_ERR_OPERATION_DENIED, "%s",
                       _("domain is not in running state"));
        goto cleanup;
    }

    if (!cont->may_control(cont)) {
	virReportError(VIR_ERR_OPERATION_DENIED, "%s",
		       _("domain may not be controlled"));
	goto cleanup;
    }

    if (!criuExists()) {
        virReportError(VIR_ERR_OPERATION_DENIED, "%s",
                       _("criu binary not found in PATH"));
        goto cleanup;
    }

    if ((save_path = concatPaths(to, domain->name)) == NULL)
        goto cleanup;

    if (virFileExists(save_path)) {
        virReportError(VIR_ERR_INVALID_ARG, "%s",
                       _("already a checkpoint present in directory"));
        goto cleanup;
    }

    if (!mkdir(save_path, S_IWUSR | S_IRUSR | S_IRGRP) < 0) {
        virReportError(VIR_ERR_OPERATION_FAILED,
                       _("failes to create directoryr '%s'"),
                       save_path);
        goto cleanup;
    }

    if (flags & VIR_DOMAIN_SAVE_RUNNING) {
        if(!cont->checkpoint(cont, save_path, false, false))
            virReportError(VIR_ERR_OPERATION_FAILED, "%s",
                           _("lxc api call failed. check lxc log for more information"));
            goto cleanup;
    } else {
        if(!cont->checkpoint(cont, save_path, true, false)) {
            virReportError(VIR_ERR_OPERATION_FAILED, "%s",
                           _("lxc api call failed. check lxc log for more information"));
            goto cleanup;
        } else {
            vm->def->id = -1;
            virDomainObjSetState(vm, VIR_DOMAIN_SHUTOFF, VIR_DOMAIN_SHUTOFF_SAVED);
            domain->id = -1;
        }
    }
    ret = 0;

 cleanup:
    if (vm)
        virObjectUnlock(vm);
    VIR_FREE(save_path);
    return ret;
}

static int
lxctoolsDomainSave(virDomainPtr domain, const char* to)
{
    return lxctoolsDomainSaveFlags(domain, to, NULL, 0);
}

static int
lxctoolsDomainShutdownFlags(virDomainPtr dom, unsigned int flags)
{
    struct lxctools_driver *driver = dom->conn->privateData;
    virDomainObjPtr vm;
    struct lxc_container* cont;
    int ret = -1;
    virCheckFlags(0, -1);

    vm = virDomainObjListFindByName(driver->domains, dom->name);

    if(!vm) {
        virReportError(VIR_ERR_NO_DOMAIN,
                        _("no domain with name '%s'"), dom->name);
        goto cleanup;
    }

    cont = vm->privateData;

    if (!cont->is_running(cont)) {
        virReportError(VIR_ERR_OPERATION_DENIED, "%s",
                       _("domain is not in running state"));
        goto cleanup;
    }

    if (!cont->stop(cont)) {
        goto cleanup;
    }

    vm->def->id = -1;
    virDomainObjSetState(vm, VIR_DOMAIN_SHUTOFF, VIR_DOMAIN_SHUTOFF_SHUTDOWN);
    dom->id = -1;
    ret = 0;

 cleanup:
    if (vm)
        virObjectUnlock(vm);
    return ret;
}

static int
lxctoolsDomainDestroy(virDomainPtr dom)
{
    return lxctoolsDomainShutdownFlags(dom, 0);
}

static int
lxctoolsDomainDestroyFlags(virDomainPtr dom, unsigned int flags)
{
    return lxctoolsDomainShutdownFlags(dom, flags);
}

static int
lxctoolsDomainShutdown(virDomainPtr dom)
{
    return lxctoolsDomainShutdownFlags(dom, 0);
}

static int
lxctoolsDomainCreateWithFlags(virDomainPtr dom, unsigned int flags)
{
    struct lxctools_driver *driver = dom->conn->privateData;
    virDomainObjPtr vm;
    struct lxc_container* cont;
    const char *prog[] = {"lxc-start", "-d", "-n", dom->name, NULL};
    virCheckFlags(0, -1);

    vm = virDomainObjListFindByName(driver->domains, dom->name);

    if(!vm) {
        virReportError(VIR_ERR_NO_DOMAIN,
                        _("no domain with name '%s'"), dom->name);
        goto cleanup;
    }

    cont = vm->privateData;

    if (cont->is_running(cont)) {
        virReportError(VIR_ERR_OPERATION_DENIED, "%s",
                       _("domain is not in shutoff state"));
        goto cleanup;
    }

    /*
    if (!cont->start(cont, 0, NULL)) {
        printf("errorcnt: %d\n", cont->error_num);
        virReportError(VIR_ERR_OPERATION_ABORTED, "%s",
                cont->error_string);
        goto cleanup;
    }*/

    if (virRun(prog, NULL) < 0) {
        virReportError(VIR_ERR_OPERATION_ABORTED, "%s",
                cont->error_string);
        goto cleanup;
    }

    if ((vm->pid = cont->init_pid(cont)) < 0)
        goto cleanup;
    vm->def->id = vm->pid;
    dom->id = vm->pid;

    virDomainObjSetState(vm, VIR_DOMAIN_RUNNING, VIR_DOMAIN_RUNNING_BOOTED);
    if (vm)
        virObjectUnlock(vm);
    return 0;
 cleanup:
    if (vm)
        virObjectUnlock(vm);
    return -1;
}

static int
lxctoolsDomainCreate(virDomainPtr dom)
{
    return lxctoolsDomainCreateWithFlags(dom, 0);
}

static int
lxctoolsDomainGetInfo(virDomainPtr dom,
                                 virDomainInfoPtr info)
{
    struct lxctools_driver *driver = dom->conn->privateData;
    struct lxc_container *cont;
    virDomainObjPtr vm;
    const char* state;
    char* config_item = NULL;
    int config_item_len;
    int ret = -1;
    vm = virDomainObjListFindByName(driver->domains, dom->name);

    if (!vm) {
        virReportError(VIR_ERR_NO_DOMAIN, "%s",
                       _("no domain with matching id"));
        goto cleanup;
    }

    cont = vm->privateData;
    state = cont->state(cont);
    info->state = lxcState2virState(state);

    /* check CPU config */
    if ((config_item_len = cont->get_config_item(cont,
                    "lxc.cgroup.cpuset.cpus", NULL, 0)) < 0)
        goto cleanup;

    if (VIR_ALLOC_N(config_item, config_item_len) < 0)
        goto cleanup;

    if (config_item_len > 0 &&
            cont->get_config_item(cont, "lxc.cgroup.cpuset.cpus",
                                  config_item, config_item_len)
            != config_item_len) {
        goto cleanup;
    }
    if ((config_item_len > 0 &&
        (info->nrVirtCpu = strtol(config_item, NULL, 10))) ||
            (info->nrVirtCpu = getNumOfHostCPUs(dom->conn)) == 0) {
        goto cleanup;
    }
    VIR_FREE(config_item);

    /* check max memory config */
    if ((config_item_len = cont->get_config_item(cont,
                    "lxc.cgroup.memory.limit_in_bytes", NULL, 0)) < 0)
        goto cleanup;

    if (VIR_ALLOC_N(config_item, config_item_len) < 0)
        goto cleanup;

    if (config_item_len > 0 &&
            cont->get_config_item(cont, "lxc.cgroup.memory.limit_in_bytes",
                                  config_item, config_item_len)
            != config_item_len) {
        goto cleanup;
    }
    if (config_item_len > 0) {
        info->maxMem = convertMemorySize(config_item, config_item_len);
    } else if ((info->maxMem = getHostMemory(dom->conn)) == 0) {
        goto cleanup;
    }
    VIR_FREE(config_item);

    if (!cont->is_running(cont)) {
        /* inactive containers do not use up any memory or cpu time */
        info->memory = 0L;
        info->cpuTime = 0L;
        ret = 0;
        goto cleanup;
    }

    /* check memory usage */
    if ((config_item_len = cont->get_cgroup_item(cont,
                    "memory.usage_in_bytes", NULL, 0)) < 0) {
        goto cleanup;
    }

    if (VIR_ALLOC_N(config_item, config_item_len) < 0)
        goto cleanup;

    if (config_item_len > 0 &&
            cont->get_cgroup_item(cont, "memory.usage_in_bytes",
                                    config_item, config_item_len)
            > config_item_len) {
        goto cleanup;
    }

    if (config_item_len > 0) {
        info->memory = (strtol(config_item, NULL, 10)>>10);
    } else {
        info->memory = 0L;
    }

    VIR_FREE(config_item);
    /* check cpu time */
    if ((config_item_len = cont->get_cgroup_item(cont,
                    "cpuacct.usage", NULL, 0)) < 0)
        goto cleanup;

    if (VIR_ALLOC_N(config_item, config_item_len) < 0)
        goto cleanup;

    if (config_item_len > 0 &&
            cont->get_cgroup_item(cont, "cpuacct.usage",
                                  config_item, config_item_len)
            > config_item_len) {
        goto cleanup;
    }
    if (config_item_len > 0) {
       info->cpuTime = strtol(config_item, NULL, 10);
    } else {
       info->cpuTime = 0L;
    }
    ret = 0;
 cleanup:
    if(vm)
        virObjectUnlock(vm);
    VIR_FREE(config_item);
    return ret;
}

static virDomainPtr lxctoolsDomainLookupByID(virConnectPtr conn,
                                             int id)
{
    struct lxctools_driver* driver = conn->privateData;
    virDomainObjPtr obj;
    virDomainPtr dom = NULL;

    obj = virDomainObjListFindByID(driver->domains, id);

    if (!obj) {
        virReportError(VIR_ERR_NO_DOMAIN, NULL);
    } else {
        dom = virGetDomain(conn, obj->def->name, obj->def->uuid);
        if (dom)
            dom->id = obj->def->id;
    }

    if(obj)
        virObjectUnlock(obj);
    return dom;
}

static virDomainPtr lxctoolsDomainLookupByName(virConnectPtr conn,
                                               const char *name)
{
    struct lxctools_driver* driver = conn->privateData;
    virDomainObjPtr obj;
    virDomainPtr dom = NULL;

    obj = virDomainObjListFindByName(driver->domains, name);

    if (!obj) {
        virReportError(VIR_ERR_NO_DOMAIN, NULL);
    } else {
        dom = virGetDomain(conn, obj->def->name, obj->def->uuid);
        if (dom)
            dom->id = obj->def->id;
    }

    if(obj)
        virObjectUnlock(obj);
    return dom;
}

static int lxctoolsConnectListDomains(virConnectPtr conn, int *ids, int nids)
{
    struct lxctools_driver* driver = conn->privateData;
    int n;
    n = virDomainObjListGetActiveIDs(driver->domains, ids, nids,
                                     NULL, NULL);

    return n;

}

static int lxctoolsConnectListDefinedDomains(virConnectPtr conn,
                                             char **const names,
                                             int nnames)
{
    struct lxctools_driver *driver = conn->privateData;
    return virDomainObjListGetInactiveNames(driver->domains, names, nnames,
                                            NULL, NULL);
}

static int lxctoolsConnectClose(virConnectPtr conn)
{
    struct lxctools_driver *driver = conn->privateData;
    lxctoolsFreeDriver(driver);
    conn->privateData = NULL;
    return 0;
}

static int lxctoolsConnectNumOfDefinedDomains(virConnectPtr conn)
{
    struct lxctools_driver *driver = conn->privateData;
    return virDomainObjListNumOfDomains(driver->domains, false, NULL, NULL);
}

static int lxctoolsConnectNumOfDomains(virConnectPtr conn)
{
    struct lxctools_driver *driver = conn->privateData;
    return virDomainObjListNumOfDomains(driver->domains, true, NULL, NULL);
}

static virDrvOpenStatus lxctoolsConnectOpen(virConnectPtr conn,
					  virConnectAuthPtr auth ATTRIBUTE_UNUSED,
					  unsigned int flags)
{
    struct lxctools_driver *driver = NULL;
    const char* lxcpath = NULL;
    virCheckFlags(VIR_CONNECT_RO, VIR_DRV_OPEN_ERROR);

    if(conn->uri == NULL) {
       if (!(lxcpath = lxc_get_global_config_item("lxc.lxcpath"))) {
           virReportError(VIR_ERR_INTERNAL_ERROR, "%s",
                          _("could not get lxc.lxcpath config item"));
           return VIR_DRV_OPEN_DECLINED;
       }
       if (!virFileExists(lxcpath)) {
           free((void*)lxcpath);
           return VIR_DRV_OPEN_DECLINED;
       }
       if (!virFileIsDir(lxcpath)) {
           free((void*)lxcpath);
	       return VIR_DRV_OPEN_DECLINED;
       }

       if(!(conn->uri = virURIParse("lxctools:///"))) {
           goto cleanup;
       }
    } else {
       /* Is schme for 'lxctools'? */
       if(conn->uri->scheme == NULL ||
          STRNEQ(conn->uri->scheme, "lxctools"))
          return VIR_DRV_OPEN_DECLINED;

       /* Is no server name given? (local driver) */
       if (conn->uri->server != NULL)
           return VIR_DRV_OPEN_DECLINED;

       /* is path supported? */
       if (conn->uri->path != NULL &&
           STRNEQ(conn->uri->path, "/")) {
           virReportError(VIR_ERR_INTERNAL_ERROR,
                          _("Unexpected lxctools URI path '%s', try lxctools:///"),
                          conn->uri->path);
           goto cleanup;
       }
       if (!(lxcpath = lxc_get_global_config_item("lxc.lxcpath"))) {
           virReportError(VIR_ERR_INTERNAL_ERROR, "%s",
                          _("could not get lxc.lxcpath config item"));
           goto cleanup;
       }
       if (!virFileExists(lxcpath)) {
           virReportError(VIR_ERR_INTERNAL_ERROR, 
                          _("lxctools directory '%s' does not exist"),
                          lxcpath);
           goto cleanup;
       }
       if (!virFileIsDir(lxcpath)) {
           virReportError(VIR_ERR_INTERNAL_ERROR,
                          _("lxctools directory '%s' is not a directory"),
                          lxcpath);
            goto cleanup;
       }

    }

    if (VIR_ALLOC(driver) < 0)
       goto cleanup;

    driver->path = lxcpath;
    driver->domains = NULL;

    if ((driver->numOfDomains = list_all_containers(driver->path, NULL, NULL)) < 0){
       goto cleanup;
    }
    if (!(driver->domains = virDomainObjListNew())) {
       goto cleanup;
    }

    if (lxctoolsLoadDomains(driver) < 0) {
       virReportError(VIR_ERR_INTERNAL_ERROR, "%s",
                      _("error while loading domains"));

       goto cleanup;
    }

    conn->privateData = driver;

    return VIR_DRV_OPEN_SUCCESS;
 cleanup:
    free((char*)lxcpath);
    if (driver) {
        if (driver->domains)
            virObjectUnref(driver->domains);
        VIR_FREE(driver);
    }
    return VIR_DRV_OPEN_ERROR;
}

static int
lxctoolsNodeGetInfo(virConnectPtr conn ATTRIBUTE_UNUSED,
                               virNodeInfoPtr nodeinfo)
{
    return nodeGetInfo(nodeinfo);
}

static int
lxctoolsNodeGetCPUStats(virConnectPtr conn ATTRIBUTE_UNUSED,
                                   int cpuNum,
                                   virNodeCPUStatsPtr params,
                                   int *nparams,
                                   unsigned int flags)
{
    return nodeGetCPUStats(cpuNum, params, nparams, flags);
}

static int
lxctoolsNodeGetMemoryStats(virConnectPtr conn ATTRIBUTE_UNUSED,
                                      int cellNum,
                                      virNodeMemoryStatsPtr params,
                                      int *nparams,
                                      unsigned int flags)
{
    return nodeGetMemoryStats(cellNum, params, nparams, flags);
}

static int
lxctoolsNodeGetCellsFreeMemory(virConnectPtr conn ATTRIBUTE_UNUSED,
                               unsigned long long *freeMems,
                               int startCell,
                               int maxCells)
{
    return nodeGetCellsFreeMemory(freeMems, startCell, maxCells);
}

static unsigned long long
lxctoolsNodeGetFreeMemory(virConnectPtr conn ATTRIBUTE_UNUSED)
{
    unsigned long long freeMem;
    if(nodeGetMemory(NULL, &freeMem) < 00)
        return 0;
    return freeMem;
}

static int
lxctoolsNodeGetCPUMap(virConnectPtr conn ATTRIBUTE_UNUSED,
                      unsigned char **cpumap,
                      unsigned int *online,
                      unsigned int flags)
{
    return nodeGetCPUMap(cpumap, online, flags);
}

static virHypervisorDriver lxctoolsHypervisorDriver = {
    .name = "LXCTOOLS",
    .connectOpen = lxctoolsConnectOpen, /* 0.0.1 */
    .connectNumOfDomains = lxctoolsConnectNumOfDomains, /* 0.0.1 */
    .connectClose = lxctoolsConnectClose, /* 0.0.2 */
    .connectListDomains = lxctoolsConnectListDomains, /* 0.0.2 */
    .domainLookupByID = lxctoolsDomainLookupByID, /* 0.0.2 */
    .domainGetInfo = lxctoolsDomainGetInfo, /* 0.0.2 */
    .connectNumOfDefinedDomains = lxctoolsConnectNumOfDefinedDomains, /* 0.0.2 */
    .connectListDefinedDomains = lxctoolsConnectListDefinedDomains, /* 0.0.2 */
    .domainLookupByName = lxctoolsDomainLookupByName, /* 0.0.2 */
    .nodeGetInfo = lxctoolsNodeGetInfo, /* 0.0.3 */
    .nodeGetCPUStats = lxctoolsNodeGetCPUStats, /* 0.0.3 */
    .nodeGetMemoryStats = lxctoolsNodeGetMemoryStats, /* 0.0.3 */
    .nodeGetCellsFreeMemory = lxctoolsNodeGetCellsFreeMemory, /* 0.0.3 */
    .nodeGetFreeMemory = lxctoolsNodeGetFreeMemory, /* 0.0.3 */
    .nodeGetCPUMap = lxctoolsNodeGetCPUMap, /* 0.0.3 */
    .domainCreate = lxctoolsDomainCreate, /* 0.0.4 */
    .domainCreateWithFlags = lxctoolsDomainCreateWithFlags, /* 0.0.4 */
    .domainShutdown = lxctoolsDomainShutdown, /* 0.0.5 */
    .domainShutdownFlags = lxctoolsDomainShutdownFlags, /* 0.0.5 */
    .domainDestroy = lxctoolsDomainDestroy, /* 0.0.5 */
    .domainDestroyFlags = lxctoolsDomainDestroyFlags, /* 0.0.5 */
    .domainRestore = lxctoolsDomainRestore, /* 0.0.6 */
    .domainRestoreFlags = lxctoolsDomainRestoreFlags, /* 0.0.6 */
    .domainSave = lxctoolsDomainSave, /* 0.0.6 */
    .domainSaveFlags = lxctoolsDomainSaveFlags, /* 0.0.6 */
    .domainMigrateBegin3Params = lxctoolsDomainMigrateBegin3Params, /* 0.0.7 */
    .domainMigratePrepare3Params = lxctoolsDomainMigratePrepare3Params, /* 0.0.7 */
    .domainMigratePerform3Params = lxctoolsDomainMigratePerform3Params, /* 0.0.7 */
    .domainMigrateFinish3Params = lxctoolsDomainMigrateFinish3Params, /* 0.0.7 */
    .domainMigrateConfirm3Params = lxctoolsDomainMigrateConfirm3Params, /* 0.0.7 */
};

static virConnectDriver lxctoolsConnectDriver = {
    .hypervisorDriver = &lxctoolsHypervisorDriver,
};

int lxctoolsRegister(void)
{
    return virRegisterConnectDriver(&lxctoolsConnectDriver,
                                    false);
}