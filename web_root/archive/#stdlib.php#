<?php
/***
 * First Version: 16-JAN-2013
 */
include("config.php");

/*
 * Standard Library of audio log functions
 * Composed by rtav (rtav@ubicee.net)
 */

/*
 * TODO:
 * Create users, ability to upload music.
 * Logging, likely in a program called lad
 * LAD will have several options: 
 *  -- record from line-in 
 *  -- recod from stream
 *  -- record using arbitrary command (first two go through this.)
 */

function createUser($username, $password)
{
}

function changePassword($username, $oldpassword, $newpassword)
{
  if(!checkPassword($username, $oldpassword)) {
    return false;
  }
  else {
    setPassword($username, $newpassword);
  }
}

function setPassword($username, $password)
{
}

function checkPassword($username, $password)
{
  if($password == "baba") {
    return true;
  }
  else {
    return false;
  }
}

function createAlbum($username, $title)
{
}

function addTrack($username, $title, $trackPath)
{
  //add flac, ogg, mp3 at the least.
  //see if there is a program/library to allow all useful formats
  //
}

function listTracks($username, $title)
{
}

function listAlbums($username)
{
}

//Creates a zip file of all tracks in an album
//and returns a download URL.
function zipAlbum($username, $title)
{
}

function removeTrack($username, $title, $trackPath)
{
}

function getURL($username, $title, $trackpath)
{
}

/**
 * These functions allow the storage of arbitrary metadata concerning
 * a track.
 */
function getTrackMetaData($username, $title, $trackPath, $key)
{
}

function setTrackMetaData($username, $title, $trackPath, $key, $value)
{
}

function println($s)
{
  print sprintf("%s<br/>", $s);
}


/**
 * Date should be in this format:
 * 15-01-2013 00:00:00
 */
function parseDate($dateString)
{
  $pd = date_parse_from_format("j-m-Y H:i:s", $dateString);
  //return the array.
  return $pd;

}

function getMP3($sDay, $sMonth, $sYear)
{
  $path = sprintf("%s/%s", 
		  getArchivePath($sDay, $sMonth, $sYear),
		  "recording.mp3");
  return $path;
}

/**
 * This function returns multiple days of audio.
 * It uses getAudioChunk to get the appropriate chunk of
 * each day.
 */
function getMultiDayAudioChunk($dayS, $monthS, $yearS,
			       $dayE, $monthE, $yearE,
			       $startH, $startM, $startS,
			       $endH, $endM, $endS)
{
  println("Returning multi-day audio chunk...");
  //construct interval
  $start = new DateTime();
  $start->setDate($yearS, $monthS, $dayS);

  $end = new DateTime();
  $start->setDate($yearE, $monthE, $dayE);

  $interval = $start->diff($end);
  $oneDay = new DateInterval('P1D'); //I find this so very ugly. --rtav

  //how many days do we have between?
  $days = $interval->days;
  $currentDay = $start->add($oneDay); //move to second day
  $days = $days-2; //remove first day and last day

  //first chunk
  //check each chunk for false, and if false, we cannot do the request.
  $chunks = array();
  $chunks[] = getAudioChunk($dayS, $monthS, $yearS,
			    $startH, $startM, $startS,
			    24, 00, 00);
  if($chunks[0] == false) {
    return false;
  }
  while($days > 0) {
    $nextYear = $currentDay->format('Y');
    $nextMonth = $currentDay->format('m');
    $nextDay = $currentDay->format('d');
    //for the whole day we don't need to process
    //just use the day's audio file.
    $chunks[] = sprintf("%s/audio.mp3", getArchivePath($nextDay, $nextMonth, $nextYear));
    /*    $chunks[] = getAudioChunk($nextDay, $nextMonth, $nextYear,
			       0, 0, 0,
			       24, 0, 0);                                                
    */
    $currentDay = $currentDay->add($oneDay);
    $days = $days - 1;
  }

   //last chunk
  $chunks[] = getAudioChunk($dayE, $monthE, $yearE,
			    0, 0, 0,
			    $endH, $endM, $endS);
  if($chunks[count($chunks)-1] == false) {
    return false;
  }
  //now, combine all chunks.
  //return combined chunks as a single file.
  return joinMP3s($chunks);
  
}

/*
 *TODO:
 *Make this function...you know...work...
 */

function joinMP3s($chunks)
{
  println("Joining mp3s.");
  //$chunks is an array of paths to MP3s.
  $chunkList = "";
  foreach($chunks as $chunk) {
    $chunkList = sprintf("%s %s", $chunkList, $chunk);
  }

  global $multi_day_log;
  $outputFile = sprintf("%s/output.mp3", $multi_day_log);

  $cmd = sprintf("mp3wrap %s %s", $outputFile, $chunkList);
  println("Cmd is $cmd");
  $ret = exec($cmd, $output, $retVal);
}

/**
 * This function returns a chunk of audio from a 
 * specific day.  All audio should be in audio.mp3
 * in the directory for that day.
 */
function getAudioChunk($day, $month, $year,
		       $startH, $startM, $startS,
		       $endH, $endM, $endS)
{
  $mp3 = getMP3($day, $month, $year);
  if(!file_exists($mp3)) {
    //We do not have audio for this day.  bail.
    return false;
  }
    
  $minutesStart = (($startH) * 60) + $startM;
  $minutesEnd = (($endH) * 60) + $endM;
  //time for output location
  $time = sprintf("%d.%d-%d.%d", $minutesStart, $startS,                                                                                 $minutesEnd, $endS);
  //output location
  $outputFile = sprintf("%s%s", getArchivePath($day, $month, $year),
			getChunkFilename($day, $month, $year, $minutesStart, $startS,
					 $minutesEnd, $endS));
  println("Output file is $outputFile");
  $ret = false;
  if(file_exists($outputFile)) {
    println("Chunk already exists!");
    //we already have the chunk processed, return that.
    $ret = getChunkFilename($day, $month, $year, $minutesStart, $startS,
			    $minutesEnd, $endS);
  }
  else {
    println("Chunk does not exist...splitting...");
    //cmd
    $cmd = sprintf("mp3splt -d %s/chunks %s %d.%d %d.%d",
		   getArchivePath($day, $month, $year), $mp3, $minutesStart, $startS,
		   $minutesEnd, $endS);
    println("$cmd");
    $ret = exec($cmd, $output, $retVal);
    println("Retval is $retVal");
    if($ret != 0) {
      println("Error chunking output...");
    }
    println(sprintf("Chunked output to %s", getChunkFilename($day, $month, $year, $minutesStart, $startS,
							     $minutesEnd, $endS)));
    //filename is in $output[3], parts[1]
    $parts = explode("\"", $output[3]);
    $file = $parts[1];
    $ret = $file;
    println("filename is $file");
  }
  $ret2 = sprintf("%s/%s", getArchiveRelativePath($day, $month, $year),
		 getChunkFilename($day, $month, $year, $minutesStart, $startS,
				  $minutesEnd, $endS));
  //what we should really do here is return a link to the chunk
  //if it already exists, we don't need to process it again
  return $ret2;
}

function dumpChunk($relativePath)
{
  // open the file in a binary mode
  $name = getArchiveAbsolutePath($relativePath);
  error_log($name);
  $fp = fopen($name, 'rb');

  // send the right headers
  header("Content-Type: audio/mpeg");
  header("Content-Length: " . filesize($name));

  // dump the mp3 and stop the script
  fpassthru($fp);
  //we cannot do anything more without fscking up.
  exit(0);
}

function runCommand($cmd)
{
}

function getChunkFilename($day, $month, $year, 
			  $minutesStart, $startS, $minutesEnd, $endS)
{
  //audio_00m_00s__23m_00s.mp3
  $time = sprintf("%02dm_%02ds__%02dm_%02ds", $minutesStart, $startS,                                                                                 $minutesEnd, $endS);
  return sprintf("/chunks/audio_%s.mp3", $time);
}

function getArchiveRelativePath($day, $month, $year)
{
  return sprintf("%s/%s/%s",
		 $year,
		 $month,
		 $day);
}

function getArchiveAbsolutePath($relativePath)
{
  global $audio_log;
  return sprintf("%s/%s", 
		 $audio_log,
		 $relativePath);
}

function getArchivePath($day, $month, $year)
{
  global $audio_log;
  return sprintf("%s/%s", 
		 $audio_log,
		 getArchiveRelativePath($day, $month, $year));
}

function sameDate($t1, $t2)
{
  if($t1["month"] != $t2["month"])
    return false;
  if($t1["day"] != $t2["day"])
    return false;
  if($t1["year"] != $t2["year"])
    return false;
  return true;
}
?>