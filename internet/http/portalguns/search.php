<?php
include("connect.php");

try {
	$conn = new PDO("mysql:host=$servername;dbname=$dbname", $username, $password);
	$conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
	
	$time = intval($_POST["time"]);
	if ($time == 0) {
		$time = time();
	}
	
	$stmt = $conn->prepare("SELECT id,DATE_FORMAT(timestamp,'%a %r') as time FROM log WHERE timestamp <= FROM_UNIXTIME(:time) AND keyframe = 1 ORDER BY id DESC LIMIT 30");
	$stmt->bindParam(':time', $time, PDO::PARAM_INT);
	$stmt->execute();
	$result = $stmt->fetchAll();

	//request_data()
	if (count($result) == 0) {
		echo "No Results Found<br>";
	}
	
	echo "<table align=\"center\" style=\"width:100%\"><tr>";
		
	$count = 0;
	foreach ($result as $item){
		
    echo "<td><a class=\"tablelink\" onclick=id_load(" . $item['id'] . ")>" . $item['time'] . "</a></td>";
	
	if ($count % 2){
		echo "</tr><tr>";
	}
	$count += 1;
	}
	echo "</tr></table><br>";
	
}
catch (PDOException $e) {
	print "Error: " . $e->getMessage();
}
$conn = null;

?>