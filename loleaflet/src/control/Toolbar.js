/* -*- js-indent-level: 8 -*- */
/*
 * Toolbar handler
 */

/* global $ window vex brandProductName _ _UNOTARGET*/
L.Map.include({

	// a mapping of uno commands to more readable toolbar items
	unoToolbarCommands: [
		'.uno:StyleApply',
		'.uno:CharFontName'
	],

	_modalDialogOptions: {
		overlayClose:true,
		opacity: 80,
		overlayCss: {
			backgroundColor : '#000'
		},
		containerCss: {
			overflow : 'hidden',
			backgroundColor : '#fff',
			padding : '20px',
			border : '2px solid #000'
		}
	},

	applyFont: function (fontName) {
		if (this.getPermission() === 'edit') {
			var msg = 'uno .uno:CharFontName {' +
				'"CharFontName.FamilyName": ' +
					'{"type": "string", "value": "' + fontName + '"}}';
			this._socket.sendMessage(msg);
		}
	},

	applyFontSize: function (fontSize) {
		if (this.getPermission() === 'edit') {
			var msg = 'uno .uno:FontHeight {' +
				'"FontHeight.Height": ' +
				'{"type": "float", "value": "' + fontSize + '"}}';
			this._socket.sendMessage(msg);
		}
	},

	getToolbarCommandValues: function (command) {
		if (this._docLayer) {
			return this._docLayer._toolbarCommandValues[command];
		}

		return undefined;
	},

	downloadAs: function (name, format, options, id) {
		if (this._fatal) {
			return;
		}

		id = id || 'export'; // not any special download, simple export

		if ((id === 'print' && this['wopi'].DisablePrint) ||
		    (id === 'export' && this['wopi'].DisableExport)) {
			this.hideBusy();
			return;
		}

		if (format === undefined || format === null) {
			format = '';
		}
		if (options === undefined || options === null) {
			options = '';
		}

		this.showBusy(_('Downloading...'), false);
		this._socket.sendMessage('downloadas ' +
			'name=' + encodeURIComponent(name) + ' ' +
			'id=' + id + ' ' +
			'format=' + format + ' ' +
			'options=' + options);
	},

	print: function () {
		if (window.ThisIsTheiOSApp) {
			window.webkit.messageHandlers.lool.postMessage('PRINT', '*');
		} else {
			this.showBusy(_('Downloading...'), false);
			this.downloadAs('print.pdf', 'pdf', null, 'print');
		}
	},

	saveAs: function (url, format, options) {
		if (url === undefined || url == null) {
			return;
		}
		if (format === undefined || format === null) {
			format = '';
		}
		if (options === undefined || options === null) {
			options = '';
		}

		this.showBusy(_('Saving...'), false);
		this._socket.sendMessage('saveas ' +
			'url=wopi:' + encodeURIComponent(url) + ' ' +
			'format=' + format + ' ' +
			'options=' + options);
	},

	renameFile: function (filename) {
		if (!filename) {
			return;
		}
		this.showBusy(_('Renaming...'), false);
		this._socket.sendMessage('renamefile filename=' + encodeURIComponent(filename));
	},

	applyStyle: function (style, familyName) {
		if (!style || !familyName) {
			this.fire('error', {cmd: 'setStyle', kind: 'incorrectparam'});
			return;
		}
		if (this._permission === 'edit') {
			var msg = 'uno .uno:StyleApply {' +
					'"Style":{"type":"string", "value": "' + style + '"},' +
					'"FamilyName":{"type":"string", "value":"' + familyName + '"}' +
					'}';
			this._socket.sendMessage(msg);
		}
	},

	applyLayout: function (layout) {
		if (!layout) {
			this.fire('error', {cmd: 'setLayout', kind: 'incorrectparam'});
			return;
		}
		if (this._permission === 'edit') {
			var msg = 'uno .uno:AssignLayout {' +
					'"WhatPage":{"type":"unsigned short", "value": "' + this.getCurrentPartNumber() + '"},' +
					'"WhatLayout":{"type":"unsigned short", "value": "' + layout + '"}' +
					'}';
			this._socket.sendMessage(msg);
		}
	},

	save: function(dontTerminateEdit, dontSaveIfUnmodified) {
		this._socket.sendMessage('save' +
		                         ' dontTerminateEdit=' + (dontTerminateEdit ? 1 : 0) +
		                         ' dontSaveIfUnmodified=' + (dontSaveIfUnmodified ? 1 : 0));
	},

	sendUnoCommand: function (command, json) {
		if (this._permission === 'edit') {
			// Add by Firefly <firefly@ossii.com.tw>
			command = command.trim(); // 去掉前後空白，(不知為何，就有程序愛加空白在命令列後面 XD)
			// 是否有替代 uno?
			var targetURL = _UNOTARGET(command, this.getDocType());
			// 有的話就用替代 uno
			if (targetURL !== '') command = targetURL;
			// 有的 uno 用 URI 方式傳遞參數，所以必須 encode 確保參數傳遞正確
			if (command.startsWith('.uno:')) {
				 command = encodeURI(command);
			}
			//----------------------------------------
			this._socket.sendMessage('uno ' + command + (json ? ' ' + JSON.stringify(json) : ''));
		}
	},

	toggleCommandState: function (unoState) {
		if (this._permission === 'edit') {
			// Modify by Firefly <firefly@ossii.com.tw>
			// Support for commands beginning with macro://
			if (!unoState.startsWith('.uno:') && !unoState.startsWith('macro://')) {
				unoState = '.uno:' + unoState;
			}
			this.sendUnoCommand(unoState);
		}
	},

	insertFile: function (file) {
		this.fire('insertfile', {file: file});
	},

	insertURL: function (url) {
		this.fire('inserturl', {url: url});
	},

	cellEnterString: function (string) {
		var command = {
			'StringName': {
				type: 'string',
				value: string
			},
			'DontCommit': {
				type: 'boolean',
				value: true
			}
		};

		this.sendUnoCommand('.uno:EnterString ', command);
	},

	renderFont: function (fontName) {
		this._socket.sendMessage('renderfont font=' + window.encodeURIComponent(fontName));
	},

	showLOKeyboardHelp: function() {
		var w;
		var iw = window.innerWidth;
		if (iw < 768) {
			w = iw - 30;
		}
		else if (iw > 1920) {
			w = 960;
		}
		else {
			w = iw / 5 + 590;
		}
		var map = this;
		$.get('loleaflet-help.html', function(data) {
			vex.open({
				content: data,
				showCloseButton: true,
				escapeButtonCloses: true,
				overlayClosesOnClick: true,
				contentCSS: {width: w + 'px'},
				buttons: {},
				afterOpen: function($vexContent) {
					map.enable(false);
					// Display help according to document opened
					if (map.getDocType() === 'text') {
						document.getElementById('text-shortcuts').style.display='block';
					}
					else if (map.getDocType() === 'spreadsheet') {
						document.getElementById('spreadsheet-shortcuts').style.display='block';
					}
					else if (map.getDocType() === 'presentation' || map.getDocType() === 'drawing') {
						document.getElementById('presentation-shortcuts').style.display='block';
					}

					// Lets translate
					var i, max;
					var translatableContent = $vexContent.find('h1');
					for (i = 0, max = translatableContent.length; i < max; i++) {
						translatableContent[i].firstChild.nodeValue = translatableContent[i].firstChild.nodeValue.toLocaleString();
					}
					translatableContent = $vexContent.find('h2');
					for (i = 0, max = translatableContent.length; i < max; i++) {
						translatableContent[i].firstChild.nodeValue = translatableContent[i].firstChild.nodeValue.toLocaleString();
					}
					translatableContent = $vexContent.find('td');
					for (i = 0, max = translatableContent.length; i < max; i++) {
						translatableContent[i].firstChild.nodeValue = translatableContent[i].firstChild.nodeValue.toLocaleString();
					}
					translatableContent = $vexContent.find('p');
					for (i = 0, max = translatableContent.length; i < max; i++) {
						translatableContent[i].firstChild.nodeValue = translatableContent[i].firstChild.nodeValue.toLocaleString();
					}

					$('.vex-content').attr('tabindex', -1);
					$('.vex-content').focus();
					// workaround for https://github.com/HubSpot/vex/issues/43
					$('.vex-overlay').css({ 'pointer-events': 'none'});
					$('.vex').click(function() {
						vex.close($vexContent.data().vex.id);
					});
					$('.vex-content').click(function(e) {
						e.stopPropagation();
					});
				},
				beforeClose: function () {
					map.focus();
					map.enable(true);
				}
			});
		});
	},

	showLOAboutDialog: function() {
		// Move the div sitting in 'body' as vex-content and make it visible
		var content = $('#about-dialog').clone().css({display: 'block'});
		// fill product-name and product-string
		var productName = (typeof brandProductName !== 'undefined') ? brandProductName : 'LibreOffice Online';
		content.find('#product-name').text(productName);
		var productString = _('This version of %productName is powered by');
		content.find('#product-string').text(productString.replace('%productName', productName));
		var w;
		var iw = window.innerWidth;
		if (iw < 768) {
			w = iw - 30;
		}
		else if (iw > 1920) {
			w = 960;
		}
		else {
			w = iw / 5 + 590;
		}
		var map = this;
		var handler = function(event) {
			if (event.keyCode === 68) {
				map._docLayer.toggleTileDebugMode();
			}
		};
		vex.open({
			content: content,
			showCloseButton: true,
			escapeButtonCloses: true,
			overlayClosesOnClick: true,
			contentCSS: { width: w + 'px'},
			buttons: {},
			afterOpen: function($vexContent) {
				map.enable(false);
				$(window).bind('keyup.vex', handler);
				// workaround for https://github.com/HubSpot/vex/issues/43
				$('.vex-overlay').css({ 'pointer-events': 'none'});
				$('.vex').click(function() {
					vex.close($vexContent.data().vex.id);
				});
				$('.vex-content').click(function(e) {
					e.stopPropagation();
				});
			},
			beforeClose: function () {
				$(window).unbind('keyup.vex', handler)
				map.enable(true);
				map.focus();
			}
		});
	}
});
