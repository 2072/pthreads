/*
  +----------------------------------------------------------------------+
  | pthreads                                                             |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2012                               		 |
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
#ifndef HAVE_PTHREADS_CLASS_STACKABLE_H
#define HAVE_PTHREADS_CLASS_STACKABLE_H
PHP_METHOD(Stackable, wait);
PHP_METHOD(Stackable, notify);
PHP_METHOD(Stackable, isRunning);
PHP_METHOD(Stackable, isWaiting);

ZEND_BEGIN_ARG_INFO_EX(Stackable_run, 0, 0, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(Stackable_wait, 0, 0, 0)
	ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(Stackable_notify, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(Stackable_isRunning, 0, 0, 0)
ZEND_END_ARG_INFO()
ZEND_BEGIN_ARG_INFO_EX(Stackable_isWaiting, 0, 0, 0)
ZEND_END_ARG_INFO()

zend_function_entry pthreads_stackable_methods[] = {
	PHP_ABSTRACT_ME(Stackable, run, Stackable_run)
	PHP_ME(Stackable, wait, Stackable_wait, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(Stackable, notify, Stackable_notify, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(Stackable, isRunning, Stackable_isRunning, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(Stackable, isWaiting, Stackable_isWaiting, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	{NULL, NULL, NULL}
};
#else
#	ifndef HAVE_PTHREADS_CLASS_STACKABLE
#	define HAVE_PTHREADS_CLASS_STACKABLE
/* {{{ proto boolean Stackable::wait([long timeout]) 
		Will cause the calling thread to wait for notification from the referenced Stackable
		When a timeout is used an reached boolean false will return
		When no timeout is used a boolean indication of success will return */
PHP_METHOD(Stackable, wait)
{
	PTHREAD thread = PTHREADS_FETCH;
	long timeout = 0L;
	
	if(ZEND_NUM_ARGS()){
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &timeout)!=SUCCESS) {
			RETURN_FALSE;
		}
	}
	
	if (thread) {
		RETURN_BOOL(pthreads_set_state_ex(thread, PTHREADS_ST_WAITING, timeout TSRMLS_CC));
	} else zend_error(E_ERROR, "pthreads has experienced an internal error while preparing to wait for a %s and cannot continue", PTHREADS_NAME);
} /* }}} */

/* {{{ proto boolean Stackable::notify()
		Send notification to everyone waiting on the Stackable
		Will return a boolean indication of success */
PHP_METHOD(Stackable, notify)
{
	PTHREAD thread = PTHREADS_FETCH;
	if (thread) {
		RETURN_BOOL(pthreads_unset_state(thread, PTHREADS_ST_WAITING TSRMLS_CC));
	}else zend_error(E_ERROR, "pthreads has experienced an internal error while preparing to notify a %s and cannot continue", PTHREADS_NAME);
	RETURN_FALSE;
} /* }}} */

/* {{{ proto boolean Stackable::isRunning() 
	Will return true while the referenced Stackable is being executed by a Worker */
PHP_METHOD(Stackable, isRunning)
{
	PTHREAD thread = PTHREADS_FETCH;
	
	if (thread) {
		RETURN_BOOL(pthreads_state_isset(thread->state, PTHREADS_ST_RUNNING TSRMLS_CC));
	} else zend_error(E_ERROR, "pthreads has experienced an internal error while preparing to read the state of a %s and cannot continue", PTHREADS_NAME);
} /* }}} */

/* {{{ proto boolean Stackable::isWaiting() 
	Will return true if the referenced Stackable is waiting for notification */
PHP_METHOD(Stackable, isWaiting)
{
	PTHREAD thread = PTHREADS_FETCH;
	
	if (thread) {
		RETURN_BOOL(pthreads_state_isset(thread->state, PTHREADS_ST_WAITING TSRMLS_CC));
	} else zend_error(E_ERROR, "pthreads has experienced an internal error while preparing to read the state of a %s and cannot continue", PTHREADS_NAME);
} /* }}} */
#	endif
#endif
