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
#ifndef HAVE_PTHREADS_STORE
#define HAVE_PTHREADS_STORE

#ifndef HAVE_PTHREADS_STORE_H
#	include <src/store.h>
#endif

#ifndef HAVE_PTHREADS_THREAD_H
#	include <src/thread.h>
#endif

#ifndef HAVE_PTHREADS_OBJECT_H
#	include <src/object.h>
#endif

#ifndef HAVE_PTHREADS_GLOBALS_H
#	include <src/globals.h>
#endif

typedef struct _pthreads_storage {
	void		*data;
	size_t 		length;
	zend_uchar 	type;
	zend_bool 	exists;
	long		lval;
	double		dval;
} *pthreads_storage;

/* {{{ statics */
static pthreads_storage pthreads_store_create(zval *pzval TSRMLS_DC);
static int pthreads_store_convert(pthreads_storage storage, zval *pzval TSRMLS_DC);
static int pthreads_store_tostring(zval *pzval, char **pstring, size_t *slength TSRMLS_DC);
static int pthreads_store_tozval(zval *pzval, char *pstring, size_t slength TSRMLS_DC);
static void pthreads_store_storage_dtor (pthreads_storage *element);
/* }}} */

/* {{{ allocate storage for an object */
pthreads_store pthreads_store_alloc(TSRMLS_D) {
	pthreads_store store = calloc(1, sizeof(*store));
	
	if (store) {
		if (zend_ts_hash_init(&store->table, 32, NULL, (dtor_func_t) pthreads_store_storage_dtor, 1)==SUCCESS){	
			if ((store->lock = pthreads_lock_alloc(TSRMLS_C))) {
				return store;
			} 
			zend_ts_hash_destroy(&store->table);
		} 
		free(store);
	}
	return NULL;
} /* }}} */

/* {{{ delete a value from the store */
int pthreads_store_delete(pthreads_store store, char *key, int keyl TSRMLS_DC) {
	int result = FAILURE;
	zend_bool locked;
	
	if (store) {
		if (pthreads_lock_acquire(store->lock, &locked TSRMLS_CC)) {
			if (zend_ts_hash_exists(&store->table, key, keyl)) {
				if (zend_ts_hash_del(&store->table, key, keyl)!=SUCCESS) {
					result = FAILURE;
				} else result = SUCCESS;
			} else result = SUCCESS;
			pthreads_lock_release(store->lock, locked TSRMLS_CC);
		} else result = FAILURE;
	}
	return result;
}
/* }}} */

/* {{{ isset helper for handlers */
zend_bool pthreads_store_isset(pthreads_store store, char *key, int keyl, int has_set_exists TSRMLS_DC) {
	zend_bool locked = 0, isset = 0;
	if (store) {
		if (pthreads_lock_acquire(store->lock, &locked TSRMLS_CC)) {
			pthreads_storage *storage;
			if (zend_ts_hash_find(&store->table, key, keyl, (void**)&storage)==SUCCESS) {
				isset=((*storage)->exists);
			}
			pthreads_lock_release(store->lock, locked TSRMLS_CC);
		}
	}
	return isset;
} /* }}} */

/* {{{ read a value from store */
int pthreads_store_read(pthreads_store store, char *key, int keyl, zval **read TSRMLS_DC) {
	zend_bool locked = 0;
	int result = FAILURE;
	if (store) {
		if (pthreads_lock_acquire(store->lock, &locked TSRMLS_CC)) {
			pthreads_storage *storage = NULL;
			if (zend_ts_hash_find(&store->table, key, keyl, (void**)&storage)==SUCCESS && storage) {
				MAKE_STD_ZVAL(*read);
				if ((result = pthreads_store_convert(*storage, *read TSRMLS_CC))!=SUCCESS) {
					FREE_ZVAL(*read);
				} else Z_SET_REFCOUNT_PP(read, 0);
			}
			pthreads_lock_release(store->lock, locked TSRMLS_CC);
		}
	}
	return result;
} /* }}} */

/* {{{ write a value to store */
int pthreads_store_write(pthreads_store store, char *key, int keyl, zval **write TSRMLS_DC) {
	int result = FAILURE;
	zend_bool locked;
	if (store) {
		pthreads_storage storage = pthreads_store_create(*write TSRMLS_CC);
		if (storage) {
			if (pthreads_lock_acquire(store->lock, &locked TSRMLS_CC)) {
				if (zend_ts_hash_update(&store->table, key, keyl, (void**) &storage, sizeof(pthreads_storage), NULL)==SUCCESS) {
					result = SUCCESS;
				} else free(store);
				pthreads_lock_release(store->lock, locked TSRMLS_CC);
			}
		}
	}
	return result;
} /* }}} */

/* {{{ seperate a zval using internals */
int pthreads_store_separate(zval * pzval, zval **separated, zend_bool allocate TSRMLS_DC) {
	int result = FAILURE;
	pthreads_storage storage;
	
	if (pzval) {	
		storage = pthreads_store_create(pzval TSRMLS_CC);
		if (storage) {
			if (allocate)
				MAKE_STD_ZVAL(*separated);
			result = pthreads_store_convert(
				storage, *separated TSRMLS_CC
			);
			pthreads_store_storage_dtor(&storage);
		}
	}
	return result;
} /* }}} */

/* {{{ seperate a zval pointer using internals */
int pthreads_store_separate_pointer(zval **ppzval, zval **separated, zend_bool allocate TSRMLS_DC) {
	return FAILURE;
} /* }}} */

/* {{{ free store storage for a thread */
void pthreads_store_free(pthreads_store store TSRMLS_DC){
	if (store) {
		zend_bool locked;
		if (pthreads_lock_acquire(store->lock, &locked TSRMLS_CC)) {
			zend_ts_hash_destroy(&store->table);
			pthreads_lock_release(store->lock, locked TSRMLS_CC);
		}
		pthreads_lock_free(store->lock TSRMLS_CC);
		free(store);
	}
} /* }}} */

/* {{{ zval to string */
static int pthreads_store_tostring(zval *pzval, char **pstring, size_t *slength TSRMLS_DC) {
	smart_str *psmart = (smart_str*) calloc(1, sizeof(smart_str));
	int result = FAILURE;
	
	if (psmart) {
		php_serialize_data_t vars;
		PHP_VAR_SERIALIZE_INIT(vars);
		php_var_serialize(							
			psmart, 
			&pzval, 
			&vars TSRMLS_CC
		);
		PHP_VAR_SERIALIZE_DESTROY(vars);
		
		if (psmart) {
			*slength = psmart->len;
			if (psmart->len) {
				*pstring = calloc(1, psmart->len+1);
				if (*pstring) {
					memmove(
						(char*) *pstring, (const void*) psmart->c, psmart->len
					);
					result = SUCCESS;
				}
			} else *pstring = NULL;
			smart_str_free(psmart);	
			free(psmart);
		}
	}
	
	return result;
} /* }}} */

/* {{{ string to zval */
static int pthreads_store_tozval(zval *pzval, char *pstring, size_t slength TSRMLS_DC) {
	int result = SUCCESS;
	ulong refcount = Z_REFCOUNT_P(pzval);
	if (pstring) {
		const unsigned char* pointer = (const unsigned char*) pstring;
		if (pointer) {
			/*
			* Populate PHP storage from pthreads_storage
			*/
			{
				php_unserialize_data_t vars;
				PHP_VAR_UNSERIALIZE_INIT(vars);
				if (!php_var_unserialize(&pzval, &pointer, pointer+slength, &vars TSRMLS_CC)) {
					result = FAILURE;
					zval_dtor(pzval);
				}							
				PHP_VAR_UNSERIALIZE_DESTROY(vars);
			}
			if (pzval && refcount)
				Z_SET_REFCOUNT_P(pzval, refcount);
		} else result = FAILURE;
	} else result = FAILURE;
	
	return result;
} /* }}} */

/* {{{ Will storeize the zval into a newly allocated buffer which must be free'd by the caller */
static pthreads_storage pthreads_store_create(zval *unstore TSRMLS_DC){					
	pthreads_storage storage;
	
	if (unstore) {
		/*
		* Make an exact copy of the store data for internal storage
		*/
		storage = (pthreads_storage) calloc(1, sizeof(*storage));
		if (storage) {
			storage->length = 0;
			
			switch((storage->type = Z_TYPE_P(unstore))){
				case IS_NULL:
					storage->data = NULL;
					storage->exists = 0;
				break;
				
				case IS_STRING: {
					if ((storage->length = Z_STRLEN_P(unstore))) {
						storage->data = (char*) calloc(1, storage->length+1);
						memmove(
							storage->data, (const void*) Z_STRVAL_P(unstore), storage->length
						);
						storage->exists = (storage->length > 0);
					} else storage->data = NULL;
				} break;
				
				case IS_BOOL:
				case IS_LONG: {
					storage->exists = ((storage->lval=Z_LVAL_P(unstore)) > 0L);
				} break;
				
				case IS_DOUBLE: storage->exists = ((storage->dval=Z_DVAL_P(unstore)) > 0.0); break;
				
				case IS_OBJECT:
				case IS_ARRAY: {
					if (pthreads_store_tostring(unstore, (char**) &storage->data, &storage->length TSRMLS_CC)==SUCCESS) {
						if (storage->type==IS_ARRAY) {
							storage->exists = zend_hash_num_elements(Z_ARRVAL_P(unstore));
						} else storage->exists = 1;
					} else free(storage);
				} break;
				
				default: storage->exists = 0;
			}
		}
	}
	
	return storage;														
}
/* }}} */

/* {{{ Will unstoreize data into the allocated zval passed */
static int pthreads_store_convert(pthreads_storage storage, zval *pzval TSRMLS_DC){
	int result = SUCCESS;
	ulong refcount = Z_REFCOUNT_P(pzval);
	
	if (storage) {
		switch(storage->type) {
			case IS_STRING:
				if (storage->data && storage->length) {
					ZVAL_STRINGL(pzval, (char*)storage->data, storage->length, 1); 
				} else ZVAL_EMPTY_STRING(pzval);
			break;
			
			case IS_NULL: ZVAL_NULL(pzval); break;
			case IS_BOOL: ZVAL_BOOL(pzval, storage->lval); break;
			case IS_LONG: ZVAL_LONG(pzval, storage->lval); break;
			case IS_DOUBLE: ZVAL_DOUBLE(pzval, storage->dval); break;
			
			case IS_ARRAY:
			case IS_OBJECT: {
				result = pthreads_store_tozval(
					pzval, 
					(char*) storage->data, 
					storage->length TSRMLS_CC
				);
			} break;
			
			default: ZVAL_NULL(pzval);
		}
		
		if (Z_TYPE_P(pzval)!=IS_NULL && refcount)
			Z_SET_REFCOUNT_P(pzval, refcount);
	}
	return result;
}
/* }}} */

/* {{{ Will free store element */
static void pthreads_store_storage_dtor (pthreads_storage *storage){
	if (storage && (*storage)) {
		switch ((*storage)->type) {
			case IS_OBJECT:
			case IS_STRING:
			case IS_ARRAY:
				if ((*storage)->data) {
					free((*storage)->data);
				}
			break;
		}
		
		free((*storage));
	}
} /* }}} */

#endif
