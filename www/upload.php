<?php
// Passwort-Schutz (muss mit dem ESP32-Code übereinstimmen)
$secret_key = "ThisIsMySecret4WebUpload";

if ($_POST['key'] == $secret_key) {
    $target_dir = "power/"; // Ordner muss existieren und Schreibrechte haben
    if (!file_exists($target_dir)) { mkdir($target_dir, 0777, true); }
    
    $target_file = $target_dir . basename($_FILES["fileToUpload"]["name"]);
    
    if (move_uploaded_file($_FILES["fileToUpload"]["tmp_name"], $target_file)) {
        echo "Datei erfolgreich hochgeladen.";
    } else {
        echo "Fehler beim Verschieben der Datei.";
    }
} else {
    echo "Zugriff verweigert.";
}
?>