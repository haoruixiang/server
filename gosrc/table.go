package main

type LoreField struct {
	Id       int64  `json:"id"`       //自增id
	Name     string `json:"name"`     //知识领域名字
	Describe string `json:"describe"` //描述
}

type LoreProint struct {
	Id       int64  `json:"id"` //自增id
	FieldId  int64  `json:"field_id"`
	UserId   int64  `json:"user_id"` //8888为公共id
	Name     string `json:"name"`
	Describe string `json:"describe"`
	Status   int    `json:"status"`
	AddTime  int64  `json:"add_time"`
}

type RelationTable struct {
	Id       int64  `json:"id"`
	UserId   int64  `json:"user_id"`
	Name     string `json:"name"`
	Describe string `json:"describe"`
}

type LoreRelation struct {
	Id           int64 `json:"id"`
	RelationId   int64 `json:"relation_id"`
	LoreProintId int64 `json:"lore_point_id"`
	Level        int   `json:"level"`
}
