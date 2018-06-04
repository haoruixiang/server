package main

import (
	"log"
	_ "net"
	_ "net/http"
	"os"
	"path/filepath"
)

/*
 *  分析的数据
 */

/*
 *  对象：name type(int，string....)
 */

type AMainData struct {
	Files map[string]*Cfile
}

type CObject struct {
	Name    []string
	Type    string //宏、变量、函数、结构体
	Data    []byte
	Objects []*CObject
}

//文件：名字、内容、对象[宏、变量、函数、结构体]
type Cfile struct {
	Name     string
	Data     []byte
	IncNames []string
	Inc      map[string]*Cfile
	Object   map[string]*CObject
}

func (o *CObject) ParseDefine(data []byte, cid int) int {
	var fn []byte
	for {
		if cid >= len(data) {
			break
		}
		if data[cid] == 10 {
			break
		}
		if data[cid] == 32 || data[cid] == 9 {
			o.Name = append(o.Name, string(fn))
			fn = []byte("")
		} else {
			fn = append(fn, data[cid])
		}
		o.Data = append(o.Data, data[cid])
		cid++
	}
	return cid
}

func (o *CObject) ParseClass(data []byte, cid int) int {
	//分析出  变量  函数
    log.Println("ParseClass -----")
	var fn []byte
	var c1, c2, b1, b2 int
	for {
		if cid >= len(data) {
			break
		}
		if data[cid] == 10 {
		} else {
			fn = append(fn, data[cid])
		}
		if data[cid] == '(' {
			c1++
		}
		if data[cid] == ')' {
			c2++
		}
		if data[cid] == '{' {
			b1++
		}
		if data[cid] == '}' {
			b2++
		}
		if data[cid] == '{' && len(o.Name) <= 0 {
			if len(fn) > 0 {
				o.Name = append(o.Name, string(fn))
				log.Println("class name", string(fn))
				fn = []byte{}
			}
		}
		if data[cid] == ';' {
			fid := GetPrevOPChar(data, cid-1)
			if data[fid] == '}' && b1 == b2 {
				o.Data = append(o.Data, data[cid])
				break
			}
			if len(fn) > 1 {
				o.Name = append(o.Name, string(fn))
				log.Println("class pp:", string(fn))
			}
			fn = []byte{}
		}
		if data[cid] == ')' && c1 == c2 {
			if len(fn) > 0 {
				obj := &CObject{Type: "func"}
				obj.Name = append(obj.Name, string(fn))
				log.Println("func name", string(fn))
				o.Data = append(o.Data, data[cid])
				cid = obj.ParseFunc(data, cid+1)
				o.Data = append(o.Data, obj.Data...)
				o.Objects = append(o.Objects, obj)
				fn = []byte{}
			}
		} else {
			o.Data = append(o.Data, data[cid])
		}
		cid++
	}
	log.Println("class data:", string(o.Data))
	return cid
}

func (o *CObject) ParseStruct(data []byte, idx int) int {
	return 0
}

func GetNextOPChar(data []byte, cid int) int {
	for {
		if cid >= len(data) {
			return cid
		}
		if data[cid] == 32 || data[cid] == 9 || data[cid] == 10 {
		} else {
			return cid
		}
		cid++
	}
}
func GetPrevOPChar(data []byte, cid int) int {
	for {
		if cid <= 0 {
			return 0
		}
		if data[cid] == 32 || data[cid] == 9 || data[cid] == 10 {
		} else {
			return cid
		}
		cid--
	}
}

func (o *CObject) ParseFunc(data []byte, cid int) int {
	//只分析出一行 一行来 ; { 一行结尾
    log.Println("---ParseFunc---",o.Name[0])
	var fn []byte
	var c1, c2, b1, b2 int
	for {
		if cid >= len(data) {
			break
		}
		if data[cid] == '(' {
			c1++
		}
		if data[cid] == ')' {
			c2++
		}
		if data[cid] == '{' {
			b1++
		}
		if data[cid] == '}' {
			b2++
		}
		if b1 > 0 && b1 == b2 {
			fid := GetPrevOPChar(data, cid-1)
			if data[fid] == '}' {
                o.Data = append(o.Data, data[cid])
				break
			}
		}
		if data[cid] == 10 || data[cid] == '{' {
		} else {
			fn = append(fn, data[cid])
		}
		//if (data[cid] == ';' && c1 == c2) || (data[cid] == '{' && b1-b2 == 1) {
        if (data[cid] == ';' && c1 == c2) {
			o.Name = append(o.Name, string(fn))
			log.Println(string(fn))
			fn = []byte{}
		}
		if data[cid] == ')' && c1 == c2 && len(fn) > 0 {
			fid := GetNextOPChar(data, cid+1)
			if fid < len(data) && data[fid] == '{' {
				obj := &CObject{Type: "func"}
				obj.Name = append(obj.Name, string(fn))
				log.Println("func name:", string(fn))
				o.Data = append(o.Data, data[cid])
				cid = obj.ParseFunc(data, cid+1)
				o.Data = append(o.Data, obj.Data...)
				o.Objects = append(o.Objects, obj)
				fn = []byte{}
			}
		} else {
			o.Data = append(o.Data, data[cid])
		}
		cid++
	}
	//log.Println("func data:",o.Name[0], len(o.Data),string(o.Data))
	return cid
}

func (f *Cfile) ParseC() {
	var fn []byte
	llen := len(f.Data)
	idx := 0
	for {
		if idx >= llen {
			break
		}
		if f.Data[idx] == 32 || f.Data[idx] == 9 || f.Data[idx] == 10 || f.Data[idx] == ';' {
			if len(fn) > 0 {
				log.Println("---ParseC---",string(fn))
				for {
					if idx >= llen {
						break
					}
					if fn[0] == '#' {
						obj := &CObject{Type: "#"}
						obj.Name = append(obj.Name, string(fn))
						idx = obj.ParseDefine(f.Data, idx+1)
						log.Println(obj.Name)
						if len(obj.Name) > 1 {
							f.Object[obj.Name[1]] = obj
						} else {
							f.Object[obj.Name[0]] = obj
						}
						break
					}
					if string(fn) == "class" {
						obj := &CObject{Type: "class"}
						idx = obj.ParseClass(f.Data, idx+1)
						f.Object[obj.Name[0]] = obj
						break
					}
					if string(fn) == "struct" {
						obj := &CObject{Type: "struct"}
						idx = obj.ParseStruct(f.Data, idx+1)
						f.Object[obj.Name[0]] = obj
						break
					}
					if string(fn) == "typedef" {
						//
					}
					obj := &CObject{Type: "func"}
					obj.Name = append(obj.Name, string(fn))
					idx = obj.ParseFunc(f.Data, idx+1)
					f.Object[obj.Name[0]] = obj
					break
				}
			}
			fn = []byte("")
			idx++
			continue
		}
		if string(fn) == "/*" || string(fn) == "//" {
			//getFree
		}
		fn = append(fn, f.Data[idx])
		idx++
	}
}

func getFilelist(path string) []string {
	var ls []string
	err := filepath.Walk(path, func(path string, f os.FileInfo, err error) error {
		if f == nil {
			return err
		}
		if f.IsDir() {
			return nil
		}
		ls = append(ls, path)
		return nil
	})
	if err != nil {
		log.Println("ERROR", err)
	}
	return ls
}
