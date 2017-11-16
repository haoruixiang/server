package main

import (
	"log"
	_ "net"
	_ "net/http"
	"os"
)

func main() {
	log.SetFlags(log.Lshortfile | log.Lmicroseconds | log.Ldate)
	if len(os.Args) != 2 {
		log.Println("ERROR", "cmd false!")
		return
	}
}
