/*
  +----------------------------------------------------------------------+
  | pthreads                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2012                                		 |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Joe Watkins <joe.watkins@live.co.uk>                         |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PHP_PTHREADS
#define HAVE_PHP_PTHREADS

#ifndef HAVE_PTHREADS_H
#	include <src/pthreads.h>
#endif

#ifndef HAVE_PHP_PTHREADS_H
#	include <php_pthreads.h>
#endif

#ifndef HAVE_PTHREADS_STATE_H
#	include <src/state.h>
#endif

#ifndef HAVE_PTHREADS_THREAD_H
#	include <src/thread.h>
#endif

#ifndef HAVE_PTHREADS_SERIAL_H
#	include <src/serial.h>
#endif

#ifndef HAVE_PTHREADS_GLOBALS_H
#	include <src/globals.h>
#endif

#ifndef HAVE_PTHREADS_OBJECT_H
#	include <src/object.h>
#endif

#ifndef ZTS
#	error "pthreads requires that Thread Safety is enabled, add --enable-maintainer-zts to your PHP build configuration"
#endif

#if COMPILE_DL_PTHREADS
	ZEND_GET_MODULE(pthreads)
#endif

#ifndef HAVE_PTHREADS_SERIAL_H
#include <src/serial.h>
#endif

#ifndef HAVE_PTHREADS_GLOBALS_H
#include <src/globals.h>
#endif

zend_module_entry pthreads_module_entry = {
  STANDARD_MODULE_HEADER,
  PHP_PTHREADS_EXTNAME,
  NULL,
  PHP_MINIT(pthreads),
  PHP_MSHUTDOWN(pthreads),
  NULL,
  NULL,
  PHP_MINFO(pthreads),
  PHP_PTHREADS_VERSION,
  STANDARD_MODULE_PROPERTIES
};

PHP_INI_BEGIN()
	/* {{{ pthreads.max allows admins some control over how many threads a user can create */
	PHP_INI_ENTRY("pthreads.max", "0", PHP_INI_SYSTEM, NULL) /* }}} */
	/* {{{ pthreads.importing allows importing threads to be disabled where it poses a security risk */
	PHP_INI_ENTRY("pthreads.importing", "1", PHP_INI_SYSTEM, NULL) /* }}} */
PHP_INI_END()

#ifndef HAVE_PTHREADS_OBJECT_H
#	include <src/object.h>
#endif

#ifndef HAVE_PTHREADS_MODIFIERS_H
#	include <src/modifiers.h>
#endif

#ifndef PTHREADS_NAME
#	define PTHREADS_NAME Z_OBJCE_P(getThis())->name
#endif

#ifndef PTHREADS_TID
#	define PTHREADS_TID thread->tid
#endif

#ifndef PTHREADS_FRIENDLY_NAME
#	define PTHREADS_FRIENDLY_NAME PTHREADS_NAME, PTHREADS_TID
#endif

PHP_MINIT_FUNCTION(pthreads)
{
	zend_class_entry te;
	zend_class_entry me;
	zend_class_entry ce;
	zend_class_entry se;
	zend_class_entry we;
	
	REGISTER_INI_ENTRIES();

	/*
	* Global Init
	*/
	if (!PTHREADS_G(init)) 
		pthreads_globals_init();
	
	INIT_CLASS_ENTRY(te, "Thread", pthreads_thread_methods);
	te.create_object = pthreads_thread_ctor;
	te.serialize = pthreads_internal_serialize;
	te.unserialize = pthreads_internal_unserialize;
	pthreads_thread_entry=zend_register_internal_class(&te TSRMLS_CC);
	
	INIT_CLASS_ENTRY(we, "Worker", pthreads_worker_methods);
	we.create_object = pthreads_worker_ctor;
	we.serialize = pthreads_internal_serialize;
	we.unserialize = pthreads_internal_unserialize;
	pthreads_worker_entry=zend_register_internal_class(&we TSRMLS_CC);
	
	INIT_CLASS_ENTRY(se, "Stackable", pthreads_stackable_methods);
	se.create_object = pthreads_stackable_ctor;
	se.serialize = zend_class_serialize_deny;
	se.unserialize = zend_class_unserialize_deny;
	pthreads_stackable_entry=zend_register_internal_class(&se TSRMLS_CC);
	
	INIT_CLASS_ENTRY(me, "Mutex", pthreads_mutex_methods);
	me.serialize = zend_class_serialize_deny;
	me.unserialize = zend_class_unserialize_deny;
	pthreads_mutex_entry=zend_register_internal_class(&me TSRMLS_CC);
	
	INIT_CLASS_ENTRY(ce, "Cond", pthreads_condition_methods);
	ce.serialize = zend_class_serialize_deny;
	ce.unserialize = zend_class_unserialize_deny;
	pthreads_condition_entry=zend_register_internal_class(&ce TSRMLS_CC);

	if ( pthread_mutexattr_init(&defmutex)==SUCCESS ) {
#ifdef DEFAULT_MUTEX_TYPE
		pthread_mutexattr_settype(
			&defmutex, 
			DEFAULT_MUTEX_TYPE
		);
#endif
	}
	
	/*
	* Setup standard and pthreads object handlers
	*/
	zend_handlers = zend_get_std_object_handlers();
	
	memcpy(&pthreads_handlers, zend_handlers, sizeof(zend_object_handlers));
	
	pthreads_handlers.get_method = pthreads_get_method;
	pthreads_handlers.call_method = pthreads_call_method;
	pthreads_handlers.read_property = pthreads_read_property;
	pthreads_handlers.write_property = pthreads_write_property;
	pthreads_handlers.has_property = pthreads_has_property;
	pthreads_handlers.unset_property = pthreads_unset_property;
	
	pthreads_handlers.get_property_ptr_ptr = NULL;
	pthreads_handlers.get = NULL;
	pthreads_handlers.set = NULL;
	pthreads_handlers.write_dimension = NULL;
	pthreads_handlers.read_dimension = NULL;
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(pthreads)
{
	UNREGISTER_INI_ENTRIES();
	
	pthread_mutexattr_destroy(
		&defmutex
	);
	
	return SUCCESS;
}

PHP_MINFO_FUNCTION(pthreads)
{
	char numbers[10];
	
	php_info_print_table_start();
	php_info_print_table_row(2, "Version", PHP_PTHREADS_VERSION);
	if (PTHREADS_G(max)) {
		php_info_print_table_row(2, "Maximum Threads", INI_STR("pthreads.max"));
	} else php_info_print_table_row(2, "Maximum Threads", "No Limits");
	
	snprintf(numbers, sizeof(numbers), "%d", pthreads_globals_count());
	php_info_print_table_row(2, "Current Threads", numbers);
	
	snprintf(numbers, sizeof(numbers), "%d", pthreads_globals_peak());
	php_info_print_table_row(2, "Peak Threads", numbers);

	php_info_print_table_end();
}

#ifndef HAVE_PTHREADS_CLASS_THREAD
#	include <classes/thread.h>
#endif

#ifndef HAVE_PTHREADS_CLASS_WORKER
#	include <classes/worker.h>
#endif

#ifndef HAVE_PTHREADS_CLASS_STACKABLE
#	include <classes/stackable.h>
#endif

#ifndef HAVE_PTHREADS_CLASS_MUTEX
#	include <classes/mutex.h>
#endif

#ifndef HAVE_PTHREADS_CLASS_COND
#	include <classes/cond.h>
#endif

#endif
