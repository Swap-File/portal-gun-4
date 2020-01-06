<?php
include("connect.php");

try {
	$conn = new PDO("mysql:host=$servername;dbname=$dbname", $username, $password);
	$conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
	
	$id = $_POST["id"]; //current id
	$dir = $_POST["dir"];    
	
	if ($dir == 1){ //forward a frame
		$stmt = $conn->prepare("SELECT id FROM log WHERE id > :id ORDER BY id LIMIT 1");
	}elseif ($dir == -1){ //backwards a frame
		$stmt = $conn->prepare("SELECT id FROM log WHERE id < :id ORDER BY id DESC LIMIT 1");
	}elseif ($dir == 2){ //forward to keyframe
		$stmt = $conn->prepare("SELECT id FROM log WHERE id > :id AND keyframe = 1 ORDER BY id LIMIT 1");
	}elseif ($dir == -2){ //backwards to keyframe
		$stmt = $conn->prepare("SELECT id FROM log WHERE id < :id AND keyframe = 1 ORDER BY id DESC LIMIT 1");
	}
	$stmt->bindParam(':id', $id, PDO::PARAM_INT);
	$stmt->execute();
	$result = $stmt->fetchAll();
	if (count($result) != 1 ) {
		echo $id;
	}else{
		echo $result[0]["id"];
	}
	
}
catch (PDOException $e) {
	print "Error: " . $e->getMessage();
}
$conn = null;

?>