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
#ifndef HAVE_PTHREADS_OBJECT_H
#define HAVE_PTHREADS_OBJECT_H

/*
* @TODO
*	static members
*	class constants
*	implement verbose debugging mode in threads
*	try and find a way a thread can throw an exception in cid so that ThreadingException can be implemented and not just cause deadlocks
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifndef HAVE_PTHREADS_H
#	include <src/pthreads.h>
#endif

#ifndef HAVE_PTHREADS_THREAD_H
#	include <src/thread.h>
#endif

/* {{{ object creation and destruction */
zend_object_value pthreads_attach_to_context(zend_class_entry *entry TSRMLS_DC);
zend_object_value pthreads_attach_to_other(zend_class_entry *entry TSRMLS_DC);
void pthreads_detach_from_context(void * child TSRMLS_DC);
void pthreads_detach_from_other(void * child TSRMLS_DC);
void * PHP_PTHREAD_ROUTINE(void *);
/* }}} */

/*
* @NOTE
*	pthreads_state_* functions work with pthreads_state directly
*	pthreads_*_state functions work with specific PTHREAD state
*/

/* {{{ state and stack management */
int pthreads_is_worker(PTHREAD thread);
int pthreads_set_worker(PTHREAD thread, zend_bool flag);
int pthreads_set_state_ex(PTHREAD thread, int state, long timeout);
int pthreads_set_state(PTHREAD thread, int state);
int pthreads_unset_state(PTHREAD thread, int state);
int pthreads_import(PTHREAD thread, zval **return_value TSRMLS_DC);
int pthreads_stack_pop(PTHREAD thread, zval *this_ptr TSRMLS_DC);
int pthreads_stack_pop_ex(PTHREAD thread, PTHREAD work TSRMLS_DC);
int pthreads_stack_push(PTHREAD thread, PTHREAD work);
int pthreads_stack_length(PTHREAD thread);
/* }}} */

/* {{{ TSRM manipulation */
#define PTHREADS_FETCH_ALL(ls, id, type) ((type) (*((void ***) ls))[TSRM_UNSHUFFLE_RSRC_ID(id)])
#define PTHREADS_FETCH_CTX(ls, id, type, element) (((type) (*((void ***) ls))[TSRM_UNSHUFFLE_RSRC_ID(id)])->element)
#define PTHREADS_CG(ls, v) PTHREADS_FETCH_CTX(ls, compiler_globals_id, zend_compiler_globals*, v)
#define PTHREADS_CG_ALL(ls) PTHREADS_FETCH_ALL(ls, compiler_globals_id, zend_compiler_globals*)
#define PTHREADS_EG(ls, v) PTHREADS_FETCH_CTX(ls, executor_globals_id, zend_executor_globals*, v)
#define PTHREADS_SG(ls, v) PTHREADS_FETCH_CTX(ls, sapi_globals_id, sapi_globals_struct*, v)
#define PTHREADS_EG_ALL(ls) PTHREADS_FETCH_ALL(ls, executor_globals_id, zend_executor_globals*) 
/* }}} */

/* {{{ fetches a PTHREAD from a specific object in a specific context */
#define PTHREADS_FETCH_FROM_EX(object, ls) (PTHREAD) zend_object_store_get_object(object, ls) /* }}} */

/* {{{ fetches a PTHREAD from a specific object in the current context */
#define PTHREADS_FETCH_FROM(object) (PTHREAD) zend_object_store_get_object(object TSRMLS_CC) /* }}} */

/* {{{ fetches the current PTHREAD from $this */
#define PTHREADS_FETCH (PTHREAD) zend_object_store_get_object(this_ptr TSRMLS_CC) /* }}} */

/* {{{ set the joined state on the referenced PTHREAD */
#define PTHREADS_IS_JOINED(t) pthreads_state_isset(t->state, PTHREADS_ST_JOINED) /* }}} */

/* {{{ set the started state on the referenced PTHREAD */
#define PTHREADS_IS_STARTED(t) pthreads_state_isset(t->state, PTHREADS_ST_STARTED) /* }}} */

/* {{{ tell if the referenced PTHREAD is in the running state */
#define PTHREADS_IS_RUNNING(t) pthreads_state_isset(t->state, PTHREADS_ST_RUNNING) /* }}} */

/* {{{ wait on the referenced PTHREAD monitor to be notified */
#define PTHREADS_WAIT(t) pthreads_set_state(t, PTHREADS_ST_WAITING) /* }}} */

/* {{{ wait on the referenced PTHREAD monitor for a specific amount of time */
#define PTHREADS_WAIT_EX(t, l) pthreads_set_state_ex(t, PTHREADS_ST_WAITING, l) /* }}} */

/* {{{ notify the referenced PTHREAD monitor */
#define PTHREADS_NOTIFY(t) pthreads_unset_state(t, PTHREADS_ST_WAITING) /* }}} */

/* {{{ lock a threads state */
#define PTHREADS_LOCK(t) pthread_mutex_lock(t->lock) /* }}} */

/* {{{ try to lock a threads state */
#define PTHREADS_TRYLOCK(t) pthread_mutex_trylock(t->lock)  /* }}} */

/* {{{ acquire wait lock */
#define PTHREADS_WLOCK(t) pthread_mutex_lock(t->wait) /* }}} */

/* {{{ try to acquire wait lock */
#define PTHREADS_TRYWLOCK(t) pthread_mutex_trylock(t->wait) /* }}} */

/* {{{ unlock a threads state */
#define PTHREADS_UNLOCK(t) pthread_mutex_unlock(t->lock) /* }}} */

/* {{{ release wait lock */
#define PTHREADS_WUNLOCK(t) pthread_mutex_unlock(t->wait) /* }}} */

/* {{{ setup reference to self using from as the threading version of $this */
#define PTHREADS_SET_SELF(from) do{\
	self = PTHREADS_FETCH_FROM(from);\
	pthreads_copy(thread, self);\
	thread->sig = self;\
	self->sig = thread;\
	self->tls = tsrm_ls;\
}while(0) /* }}} */

/* {{{ handlers included here for access to macros above */
#ifndef HAVE_PTHREADS_HANDLERS_H
#	include <src/handlers.h>
#endif /* }}} */

#endif /* HAVE_PTHREADS_OBJECT_H */
