/* -*- js-indent-level: 8 -*- */
/*
	Manage Mac/Ip
	*/
/* global $ Admin makeMacIpColumns */

var getCookie =  function(name) {
	var cookies = document.cookie.split(';');
	for (var i = 0; i < cookies.length; i++) {
		var cookie = cookies[i].trim();
		if (cookie.indexOf(name) === 0) {
			return cookie;
		}
	}

	return '';
}
var AdminSocketMacIp= Admin.SocketBase.extend({
	constructor: function(host) {
		this.base(host);
		this._init();
	},

	_init: function() {
		$(document).ready(function() {
			if (getCookie('deftab') != '') {
				var tabid = getCookie('deftab').split('=')[1];
				if (tabid == '') {
					$('.nav-tabs a[href="#a1"]').tab('show');
				}
				else {
					if (Number(tabid.replace('#a','')) > 3)
						tabid = '#a1';
					$('.nav-tabs a[href="'+tabid+'"]').tab('show');
				}
			}
		});
	},

	onSocketOpen: function() {
		// Base class' onSocketOpen handles authentication
		this.base.call(this);
		this.socket.send('module templaterepo mac_list');
		this.socket.send('module templaterepo ip_list');
	},

	onSocketMessage: function(e) {
		var textMsg;
		if (typeof e.data === 'string')
		{
			textMsg = e.data;
		}
		else
		{
			textMsg = '';
		}

		var success;
		if (textMsg.startsWith('mac_list') ||
			textMsg.startsWith('ip_list'))
		{
			/// Mac/IP address list
			var forMac = false;
			var forIP = false;
			var type = '';
			if (textMsg.startsWith('mac_list'))
			{
				forMac = true;
				textMsg = textMsg.substring('mac_list '.length);
				var formid = '#macForm';
				type = 'mac';
			}
			if (textMsg.startsWith('ip_list'))
			{
				forIP = true;
				textMsg = textMsg.substring('ip_list '.length);
				formid = '#ipForm';
				type = 'ip';
			}
			var jsonStart = textMsg.indexOf('{');
			var jsonMsg = JSON.parse(textMsg.substr(jsonStart).trim());
			var macList = jsonMsg;

			makeMacIpColumns(formid, macList, type);

			var socketSettings = this.socket;

			var command = 'module templaterepo ';
			/// 列完後直接新增 del, modify, add event
			// modify
			$('button[name=mod_macip][ctype='+type+']').on('click', function(e)
				{
				e.preventDefault();
				var recid = this.form.elements['rec_id'].value;
				var macip = this.form.elements['macip'].value;
				var desc = this.form.elements['desc'].value;
				if (macip === '') {
					alert('請輸入 Mac/IP address');
					return;
				}

				if (forMac) {
					command += 'set_mac';
				}
				if (forIP) {
					command += 'set_ip';
				}
				command += ' ' + recid;
				command += ',' + desc;
				command += ',' + macip;
				socketSettings.send(command);
					//this.form.submit();
			});
			// del
			$('button[name=del_macip][ctype='+type+']').on('click', function(e)
				{
				e.preventDefault();
				if (!confirm('確認刪除？')) {
					return;
				}
				var recid = this.form.elements['rec_id'].value;

				if (forMac) {
					command += 'rm_mac_data';
				}
				if (forIP) {
					command += 'rm_ip_data';
				}
				command += ' ' + recid;
				socketSettings.send(command);
					//this.form.submit();
			});
			// add
			$('#macForm[ctype='+type+'], #ipForm[ctype='+type+']').on('submit', function(e)
				{
				e.preventDefault();
				var macip = this.elements['macip'].value.trim();
				var desc = this.elements['desc'].value.trim();
				if (macip === '') {
					alert('請輸入 Mac/IP address');
					return;
				}

				if (forMac) {
					command += 'add_mac_data';
				}
				if (forIP) {
					command += 'add_ip_data';
				}
				command += ' ' + macip;
				command += ',' + desc;
				socketSettings.send(command);
					//this.submit();
			});
		}
		else if (textMsg.startsWith('set_ip ') ||
			textMsg.startsWith('set_mac ')) {
			/// 更新 mac/ip address: server 傳回
			success = JSON.parse(textMsg.substring(textMsg.indexOf(' ') + 1));
			if (success) {
				alert('修改成功');
				//location.reload();
			}
			else {
				alert('修改失敗');
			}
		}
		else if (textMsg.startsWith('add_mac_data ') ||
			textMsg.startsWith('add_ip_data ')) {
			/// 新增 mac/ip address: server 傳回
			success = JSON.parse(textMsg.substring(textMsg.indexOf(' ') + 1));
			if (success) {
				alert('新增成功');
				location.reload();
			}
			else {
				alert('新增失敗');
			}
		}
		else if (textMsg.startsWith('rm_mac_data ') ||
			textMsg.startsWith('rm_ip_data ')) {
			/// 新增 mac/ip address: server 傳回
			success = JSON.parse(textMsg.substring(textMsg.indexOf(' ') + 1));
			if (success) {
				alert('刪除成功');
				location.reload();
			}
			else {
				alert('刪除失敗');
			}
		}
	},

	onSocketClose: function() {
	}
});

Admin.MacIp = function(host)
{
	return new AdminSocketMacIp(host);
};
