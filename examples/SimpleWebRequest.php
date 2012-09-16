<?php
error_reporting(E_ALL);

class AsyncWebRequest extends Thread {
	public function __construct($url){
		$this->url = $url;
	}
	
	public function run(){
		if($this->url){
			/*
			* If a large amount of data is being requested, you might want to
			* fsockopen and read using usleep in between reads
			*/
			return file_get_contents($this->url);
		} else printf("Thread #%lu was not provided a URL\n", $this->getThreadId());
	}
}

$t = microtime(true);
$g = new AsyncWebRequest(sprintf("http://www.google.com/?q=%s", rand()*10));
if($g->start()){
	printf("Request took %f seconds to start ", microtime(true)-$t);
	while($g->isBusy()){
		echo ".";
		usleep(50);
	}
	$r = $g->join();
	printf(" and %f seconds to finish receiving %d bytes\n", microtime(true)-$t, strlen($r));
}
?>