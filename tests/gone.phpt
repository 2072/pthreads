--TEST--
Test objects that have gone away
--DESCRIPTION--
This test verifies that objects that have gone away do not cause segfaults
--FILE--
<?php
class O extends Stackable { 
	public function run() {

	}
}

class T extends Thread {
	public $o;

	public function run() {
		$this->o = new O();
		/* this will disappear */
		$this->o["data"] = true;	
	}
}

$t = new T();
$t->start();
$t->join();

var_dump($t->o);
?>
--EXPECT--
object(O)#2 (0) {
}

