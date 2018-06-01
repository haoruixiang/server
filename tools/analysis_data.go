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
    var fn []byte
    cc := 0
    for {
        if cid >= len(data) {
            break
        }
        if data[cid] == ';' && cc == 0 {
            break
        }
        if data[cid] == '{' {
            cc++
        }
        if data[cid] == '}' {
            cc--
        }
        if data[cid] == 32 || data[cid] == 9 || data[cid] == 10 {
            if len(fn) > 0 {
                //log.Println(string(fn))
                o.Name = append(o.Name, string(fn))
                fn = []byte("")
            }
        } else {
            fn = append(fn, data[cid])
        }
        o.Data = append(o.Data, data[cid])
        cid++
    }
    log.Println("class data:",string(o.Data))
    return cid
}

func (o *CObject) ParseStruct(data []byte, idx int) int {
	return 0
}

func (o *CObject) ParseFunc(data []byte, cid int) int {
	var fn []byte
	cw := 0
	cc := 0
	for {
		if cid >= len(data) {
			break
		}
		if data[cid] == ';' && cw%2 == 0 && cc%2 == 0 {
            o.Name = append(o.Name, string(fn))
            //log.Println(string(fn))
			break
		}
		if data[cid] == '(' || data[cid] == ')' {
			cc++
		}
		if data[cid] == '{' || data[cid] == '}' {
			cw++
		}
		if (data[cid] == 32 || data[cid] == 9 ||  data[cid] == 10) && cc == 0 && cw%2 == 0 {
            if len(fn) > 0{
			    o.Name = append(o.Name, string(fn))
			    //log.Println(cid,len(data),string(fn))
            }
			fn = []byte("")
		} else {
			fn = append(fn, data[cid])
		}
		o.Data = append(o.Data, data[cid])
		cid++
	}
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
		//log.Println(idx,llen, string(fn) )
		if f.Data[idx] == 32 || f.Data[idx] == 9 || f.Data[idx] == 10 {
			if len(fn) > 0 {
				log.Println(string(fn))
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
                        }else{
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
