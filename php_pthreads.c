#ifndef HAVE_PHP_THREADS
#define HAVE_PHP_THREADS
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <php.h>
#include <php_globals.h>
#include <php_main.h>
#include <php_ticks.h>
#include <Zend/zend.h>
#include <Zend/zend_compile.h>
#include <Zend/zend_globals.h>
#include <Zend/zend_hash.h>
#include <Zend/zend_interfaces.h>
#include <Zend/zend_object_handlers.h>
#include <Zend/zend_vm.h>
#include <TSRM/TSRM.h>
#include "php_pthreads.h"
#include "pthreads_event.h"
#include "pthreads_object.h"

ZEND_GET_MODULE(pthreads)

zend_class_entry 		*pthreads_class_entry;
zend_class_entry 		*pthreads_mutex_class_entry;

pthread_mutexattr_t		defmutex;

zend_function_entry pthreads_methods[] = {
	PHP_ME(Thread, start, 		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(Thread, self,		NULL, ZEND_ACC_PROTECTED|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	PHP_ME(Thread, busy,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(Thread, wait,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	{NULL, NULL, NULL}
};

zend_function_entry pthreads_mutex_methods[] = {
	PHP_ME(Mutex, create,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	PHP_ME(Mutex, lock, 		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	PHP_ME(Mutex, trylock, 		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	PHP_ME(Mutex, unlock,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	PHP_ME(Mutex, destroy,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	{NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(pthreads){
	zend_class_entry te; {
		INIT_CLASS_ENTRY(
			te, "Thread", pthreads_methods
		);
		te.create_object = pthreads_attach_to_instance;
		te.serialize = zend_class_serialize_deny;
		te.unserialize = zend_class_unserialize_deny;
		pthreads_class_entry=zend_register_internal_class(&te TSRMLS_CC);
	}
	
	zend_class_entry me; {
		INIT_CLASS_ENTRY(
			me, "Mutex", pthreads_mutex_methods
		);
		me.serialize = zend_class_serialize_deny;
		me.unserialize = zend_class_unserialize_deny;
		pthreads_mutex_class_entry=zend_register_internal_class(&me TSRMLS_CC);
	}
	
	if(pthread_mutexattr_init(&defmutex)==SUCCESS){
		pthread_mutexattr_settype(
			&defmutex, 
			PTHREAD_MUTEX_ERRORCHECK_NP
		);
	}

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(pthreads){
	pthread_mutexattr_destroy(
		&defmutex
	);
	return SUCCESS;
}

/**
* @TODO Better switch on create
**/
PHP_METHOD(Thread, start){
	PTHREAD thread = PTHREADS_FETCH;
	int result = -1;
	if(thread && !thread->started->fired){
		if(zend_hash_find(&Z_OBJCE_P(getThis())->function_table, "run", sizeof("run"), (void**) &thread->runnable)==SUCCESS){	
			if((result = pthread_create(
				&thread->thread, NULL, 
				PHP_PTHREAD_ROUTINE, 
				(void*)thread
			)) == SUCCESS){
				thread->running = 1;
				pthreads_wait_event(
					thread->started
				);
			} else zend_error(E_ERROR, "Internal Error: thread creation failed, result code %d", result);
		} else zend_error(E_ERROR, "Internal Error: this Thread does not implement a run method");
	} else zend_error(E_ERROR, "Internal Error: this thread has already been started, thread objects cannot be re-used");
	if(result==SUCCESS)
		RETURN_TRUE;
	RETURN_FALSE;
}

PHP_METHOD(Thread, self){ ZVAL_LONG(return_value, (ulong) pthread_self()); }

PHP_METHOD(Thread, busy){
	PTHREAD thread = PTHREADS_FETCH;
	if(thread){
		if(thread->running){
			if(thread->finished->fired){
				RETURN_FALSE;
			} else RETURN_TRUE;
		} else zend_error(E_WARNING, "The requested thread has not yet been started");
	} else zend_error(E_ERROR, "Internal Error: failed to find thread object in instance");
	RETURN_NULL();
}

PHP_METHOD(Thread, wait) { 
	PTHREAD thread = PTHREADS_FETCH;
	if(thread){
		if(!thread->joined){
			thread->joined=1;
			switch(pthread_join(thread->thread, NULL)){
				case EINVAL: 
					zend_error(E_WARNING, "The implementation has detected that the value specified by thread does not refer to a joinable thread"); 
					RETURN_FALSE;
				break; 
				case ESRCH:
					zend_error(E_WARNING, "No thread could be found corresponding to that specified by the given thread ID");
					RETURN_FALSE;
				break;
				case EDEADLK:
					zend_error(E_ERROR, "A deadlock was detected or the value of thread specifies the calling thread");
					RETURN_FALSE;
				break;
				
				default: 
					*return_value = thread->result;
			}
		} else {
			zend_error(E_WARNING, "The implementation has detected that the value specified by thread has already been joined");
			RETURN_TRUE; 
		}
	} else {
		zend_error(E_ERROR, "Internal Error: failed to find thread object in instance");
		RETURN_FALSE;
	}
}

PHP_METHOD(Mutex, create){
	pthread_mutex_t *mutex = (pthread_mutex_t*) calloc(1, sizeof(pthread_mutex_t));
	if(mutex){
		switch(pthread_mutex_init(mutex, &defmutex)){
			case SUCCESS: RETURN_LONG((ulong)mutex); break;
			case EAGAIN:
				zend_error(E_ERROR, "The system lacked the necessary resources (other than memory) to initialise another mutex"); 
				RETURN_FALSE;
			break;
			case ENOMEM: /* I would imagine we would fail to write this message to output if we are really out of memory */
				zend_error(E_ERROR, "The system lacked the necessary resources (other than memory) to initialise another mutex"); 
				RETURN_FALSE;
			break;
			case EPERM:
				zend_error(E_ERROR, "The caller does not have the privilege to perform the operation"); 
				RETURN_FALSE;
			break;
			case EBUSY:
				zend_error(E_ERROR, "The implementation has detected an attempt to re-initialise the object referenced by mutex, a previously initialised, but not yet destroyed, mutex"); 
				RETURN_FALSE;
			break;
			
			default: 
				zend_error(E_ERROR, "Internal Error: attempt at mutex locking failed");
				RETURN_FALSE;
		}
	} else zend_error(E_ERROR, "Internal Error: failed to allocate memory for mutex");
	RETURN_NULL();
}

PHP_METHOD(Mutex, lock){
	pthread_mutex_t *mutex;
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mutex)==SUCCESS && mutex){
		switch(pthread_mutex_lock(mutex)){
			case SUCCESS: RETURN_TRUE; break;
			case EDEADLK:
				zend_error(E_WARNING, "The current thread already owns the mutex");
				RETURN_TRUE;
			break;
			case EINVAL: 
				zend_error(E_ERROR, "The value specified by mutex does not refer to an initialised mutex object"); 
				RETURN_FALSE;
			break;
			case E_ERROR: 
				zend_error(E_ERROR, "The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded");
				RETURN_FALSE;
			break;
			
			default: 
				zend_error(E_ERROR, "Internal Error: attempt at mutex locking failed");
				RETURN_FALSE;
		}
	}
	RETURN_NULL();
}

PHP_METHOD(Mutex, trylock){
	pthread_mutex_t *mutex;
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mutex)==SUCCESS && mutex){
		switch(pthread_mutex_trylock(mutex)){
			case SUCCESS: RETURN_TRUE; break;
			case EBUSY: RETURN_FALSE; break;
			
			case EDEADLK:
				zend_error(E_WARNING, "The current thread already owns the mutex");
				RETURN_TRUE;
			break;
			
			case EINVAL: 
				zend_error(E_ERROR, "The value specified by mutex does not refer to an initialised mutex object"); 
				RETURN_FALSE;
			break;
			case EAGAIN: 
				zend_error(E_ERROR, "The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded");
				RETURN_FALSE;
			break;
			
			default: 
				zend_error(E_ERROR, "Internal Error: attempt to try mutex locking failed");
				RETURN_FALSE;
		}
	}
	RETURN_NULL();
}

PHP_METHOD(Mutex, unlock){
	pthread_mutex_t *mutex;
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mutex)==SUCCESS && mutex){
		switch(pthread_mutex_unlock(mutex)){
			case SUCCESS: RETURN_TRUE; break;
			case EINVAL: 
				zend_error(E_WARNING, "The value specified by mutex does not refer to an initialised mutex object"); 
				RETURN_FALSE;
			break;
			case EPERM:
				zend_error(E_WARNING, "The current thread does not own the mutex");
			break;
			default:
				zend_error(E_ERROR, "internal error, attempt to unlock mutex failed");
				RETURN_FALSE;
		}
	}
	RETURN_NULL();
}

PHP_METHOD(Mutex, destroy){
	pthread_mutex_t *mutex;
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &mutex)==SUCCESS && mutex){
		switch(pthread_mutex_destroy(mutex)){
			case SUCCESS: 
				free(mutex);
				RETURN_TRUE;
			break;
			
			case EBUSY:
				zend_error(E_WARNING, "The implementation has detected an attempt to destroy the object referenced by mutex while it is locked or referenced"); 
				RETURN_FALSE;
			break;
			
			case EINVAL:
				zend_error(E_WARNING, "The value specified by mutex does not refer to an initialised mutex object"); 
				RETURN_FALSE;
			break;
			
			default:
				zend_error(E_ERROR, "internal error, attempt to destroy mutex failed");
				RETURN_FALSE;
		}
	}
	RETURN_NULL();
}
#endif
