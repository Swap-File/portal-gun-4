<?php
include("connect.php");

try {
    $conn = new PDO("mysql:host=$servername;dbname=$dbname", $username, $password);
    $conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
	
	$id = $_GET["id"];
    $stmt = $conn->prepare("SELECT img FROM log WHERE id=:id");
	$stmt->bindParam(':id', $id, PDO::PARAM_INT);
    $stmt->execute();
    $result = $stmt->fetchAll();
	
    if (count($result) > 0 ) {
		if (strlen($result[0]['img']) > 1){
			header("Content-Type: image/jpeg");
			echo $result[0]['img'];	
		}
		else{
			header("Content-Type: image/jpeg");
			echo file_get_contents("black.jpg");
		}
    }
}
catch (PDOException $e) {
    print "Error: " . $e->getMessage();
}
$conn = null;

?>