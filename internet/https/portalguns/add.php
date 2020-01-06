<?php
include("connect.php");

try {
    $conn = new PDO("mysql:host=$servername;dbname=$dbname", $username, $password);
    $conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    
	//get last frame
	$stmt = $conn->prepare("SELECT g_state,c_state,timestamp FROM log ORDER BY id DESC LIMIT 1");
	$stmt->execute();
    $last_result = $stmt->fetchAll();
	
	if (count($last_result) > 0 ) {
		
		$keyframe = 0;
		
		//keyframe if video is live when coming online
		if ((time() - strtotime($last_result[0]["timestamp"]) > 90) && ((intval ($_POST["g_state"]) < -1) || (intval ($_POST["c_state"]) < -1))){	
			$keyframe = 1;
		}
		//keyframe on gordon coming online
		if (intval ($_POST["g_state"]) < -1 && intval ($last_result[0]["g_state"]) >= -1){	
			$keyframe = 1;
		}
		//keyframe on chell coming online
		if (intval ($_POST["c_state"]) < -1 && intval ($last_result[0]["c_state"]) >= -1){	
			$keyframe = 1;
		}
		if (intval ($_POST["keyframe"]) == 1){
			$keyframe = 1;
		}
					
		$stmt = $conn->prepare("INSERT INTO log (img,ip,g_state,c_state,g_lag,c_lag,g_temp1,c_temp1,g_temp2,c_temp2,g_volts,c_volts,g_live,c_live,synced,keyframe)
		VALUES (:img,:ip,:g_state,:c_state,:g_lag,:c_lag,:g_temp1,:c_temp1,:g_temp2,:c_temp2,:g_volts,:c_volts,:g_live,:c_live,:synced,:keyframe)");

		$imageData = file_get_contents($_FILES["img"]["tmp_name"]);

		$stmt->bindParam(':img', $imageData, PDO::PARAM_LOB);
		$stmt->bindParam(':ip', $_SERVER['REMOTE_ADDR'], PDO::PARAM_STR);
		
		$stmt->bindParam(':g_state', $_POST["g_state"], PDO::PARAM_INT);
		$stmt->bindParam(':c_state', $_POST["c_state"], PDO::PARAM_INT);
		
		$stmt->bindParam(':g_lag', $_POST["g_lag"], PDO::PARAM_STR);
		$stmt->bindParam(':c_lag', $_POST["c_lag"], PDO::PARAM_STR);

		$stmt->bindParam(':g_temp1', $_POST["g_temp1"], PDO::PARAM_STR);
		$stmt->bindParam(':c_temp1', $_POST["c_temp1"], PDO::PARAM_STR);

		$stmt->bindParam(':g_temp2', $_POST["g_temp2"], PDO::PARAM_STR);
		$stmt->bindParam(':c_temp2', $_POST["c_temp2"], PDO::PARAM_STR);
		
		$stmt->bindParam(':g_volts', $_POST["g_volts"], PDO::PARAM_STR);
		$stmt->bindParam(':c_volts', $_POST["c_volts"], PDO::PARAM_STR);
		
		$stmt->bindParam(':g_live', $_POST["g_live"], PDO::PARAM_INT);
		$stmt->bindParam(':c_live', $_POST["c_live"], PDO::PARAM_INT);
		
		$stmt->bindParam(':synced', $_POST["synced"], PDO::PARAM_INT);
		$stmt->bindParam(':keyframe', $keyframe, PDO::PARAM_INT);
		
		$stmt->execute();
		
	}
	
	
			
    echo 'done';
}
catch (PDOException $e) {
    print "Error: " . $e->getMessage();
}
$conn = null;

?>