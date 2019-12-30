update txzj_miniapp_white_list set id = 0 where id = 1 limit 1,1;
select * from txzj_miniapp_white_list where 1 limit 1;
select * from txzj_miniapp_white_list where !(id);     