<?php

include("stdlib.php");
?>
<html>
<head>
<title>LAD</title>
<h1>LAD</h1>
<h3><i></i></h3>
</head>
<body>
Download Archive Section:

<form method="GET" action="index.php">
   Start Time: <input type="text" name="startTime" value="15-01-2013 00:00:00"/><br/>
   End Time: <input type="text" name="endTime" value="15-01-2013 01:00:00"/><br/>
   <input type="submit"/>
</form>
<?php

  $start = FALSE;
  $end = FALSE;
  if(isset($_GET["startTime"])) {
    $start = parseDate($_GET["startTime"]);
  }
  if(isset($_GET["endTime"])) {
    $end = parseDate($_GET["endTime"]);
  }

//we might want to generalize this, but for now we can check if it is the
//same day.
if(sameDate($start, $end)) {
  //same day
  $chunk = getAudioChunk($start["day"], $start["month"], $start["year"],
			 $start["hour"], $start["minute"], $start["second"],
			 $end["hour"], $end["minute"], $end["second"]);
  if($chunk == false) {
    println("Sorry, we have no archive for that day.");
  }
  else {
    println("Chunk is $chunk");
    println("<a href=\"download.php?chunk=$chunk\">Download Chunk...</a>");
  }
}
else {
  println("Entering multi-day chunk...");
  //extract for the portion of the start day, all middle days
  //and then the portion of the end day
  //Merging required, so this is a special case.
  $chunk = getMultiDayAudioChunk($start["day"], $start["month"], $start["year"],
				 $end["day"], $end["month"], $end["year"],
				 $start["hour"], $start["minute"], $start["second"],
				 $end["hour"], $end["minute"], $end["second"]);
    
}




?>
</body>
</html>