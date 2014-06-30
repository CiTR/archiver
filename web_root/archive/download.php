<?php
include("stdlib.php");

if(isset($_GET["chunk"])) {
  dumpChunk($_GET["chunk"]);
}
else {
  dumpChunk("2013/01/15/error404.mp3");
}
?>