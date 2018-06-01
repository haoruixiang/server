package main

import (
	"io/ioutil"
	"log"
	"strings"
	_ "net/http"
	"os"
	"path/filepath"
    "github.com/go-redis/redis"
)

func (m *AMainData) AnalysisC(path string) {
	files := getFilelist(path)
	for _, f := range files {
		//.cpp .c .cc .h
		//log.Println("INFO", "file:", f)
		bf := []byte(f)
		if string(bf[len(bf)-2:]) == ".c" ||
			string(bf[len(bf)-2:]) == ".h" ||
			string(bf[len(bf)-4:]) == ".cpp" ||
			string(bf[len(bf)-3:]) == ".cc" {
			log.Println("INFO", "load file:", f)
			dat, err := ioutil.ReadFile(f)
			if err != nil {
				continue
			}
			file := &Cfile{
				Name:   f,
				Data:   dat,
				Inc:    make(map[string]*Cfile),
				Object: make(map[string]*CObject),
			}
			file.ParseC()
			m.Files[f] = file
			break
		}
	}
}

func main() {
	log.SetFlags(log.Lshortfile | log.Lmicroseconds | log.Ldate)
	if len(os.Args) != 2 {
		log.Println("ERROR", "cmd false!")
		//return
	}
    ar :="mongodb://msg:vXuJtxBjxwn1ZPmP@172.16.0.208:27018,172.16.0.208:27018/115_user_yun?authSource=115_user_yun&replicaSet=immsg&authMechanism=SCRAM-SHA-1"
    log.Println(strings.Split(ar,"/")[3])
    /*client := redis.NewClient(&redis.Options{Addr:"172.16.0.34:6379",})
    log.Println(client.Ping().Result())
    d, e := client.HGet("test1","f1").Result()
    log.Println(d,"|",e)
    r := client.HMSet("test1",map[string]interface{}{"f1":100,"f4":200,"f3":102})
    log.Println(r.Result())
    log.Println(client.HMGet("test1","f1","f2","f3").Result())
    log.Println(client.HKeys("test1").Result())
    log.Println(client.HGetAll("test1").Result())
    return*/
	mMain := &AMainData{
		Files: make(map[string]*Cfile),
	}
	mMain.AnalysisC("/home/haoruixiang/server/")
}
