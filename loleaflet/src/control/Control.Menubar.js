/* -*- js-indent-level: 8 -*- */
/*
* Control.Menubar
*/

/* global $ _ _UNO vex revHistoryEnabled closebutton L */
L.Control.Menubar = L.Control.extend({
	// TODO: Some mechanism to stop the need to copy duplicate menus (eg. Help)
	options: {
		initial: [
			{name: _UNO('.uno:PickList')},
			{name: _UNO('.uno:EditMenu')},
			{name: _UNO('.uno:ViewMenu')},
			{name: _UNO('.uno:InsertMenu')},
			{name: _UNO('.uno:ToolsMenu')}
		],
		text:  [
			{name: _UNO('.uno:PickList', 'text'), id: 'file', icon:'icon_file', type: 'menu', menu: [
				{name: _UNO('.uno:Save', 'text'), id: 'save', icon:'fa fa-floppy-o', hotkey: 'Ctrl+S', type: 'action'},
				{name: _UNO('.uno:SaveAs', 'text'), id: 'saveas', type: 'action', icon:'fa fa-download'},
				{name: _('Share...'), id:'shareas', type: 'action', icon: 'fa fa-share-alt'},
				{name: _UNO('.uno:Print', 'text'), id: 'print', type: 'action', icon:'fa fa-print', hotkey: 'Ctrl+P'},
				{name: _('See revision history'), id: 'rev-history', type: 'action', icon:'fa fa-history'},
				{name: _('Download as'), id: 'downloadas', type: 'menu', icon:'fa fa-cloud-download', menu: [
					{name: _('PDF Document (.pdf)'), id: 'downloadas-pdf', type: 'action', icon: 'fa fa-file-pdf-o'},
					{name: _('TEXT Document (.txt)'), id: 'downloadas-txt', icon: 'fa fa-file-pdf-o', type: 'action'},
					{name: _('HTML Document (.html)'), id: 'downloadas-html', icon: 'fa fa-file-pdf-o', type: 'action'},
					{name: _('ODF text document (.odt)'), id: 'downloadas-odt', type: 'action', icon: 'fa fa-file-text-o'},
					{name: _('Word 2003 Document (.doc)'), id: 'downloadas-doc', type: 'action', icon: 'fa fa-file-word-o'},
					{name: _('Word Document (.docx)'), id: 'downloadas-docx', type: 'action', icon: 'fa fa-file-word-o'},
					{name: _('Rich Text (.rtf)'), id: 'downloadas-rtf', type: 'action'}]},
				{name: _('Sign document'), id: 'signdocument', type: 'action'},
				/*{type: 'separator'},
				{uno: '.uno:SetDocumentProperties'},*/
				{type: 'separator'},
				{name: _UNO('CloseDoc'), id: 'closedocument', type: 'action', icon:'fa fa-times-circle'}
			]},
			{name: _UNO('.uno:EditMenu', 'text'), type: 'menu', menu: [
				{uno: '.uno:Undo', icon: 'fa fa-undo', hotkey: 'Ctrl+Z'},
				{uno: '.uno:Redo', icon: 'fa fa-repeat', hotkey: 'Ctrl+Y'},
				{name: _('Repair'), id: 'repair',  type: 'action', icon: 'fa fa-retweet'},
				{type: 'separator'},
				{uno: '.uno:Cut', icon: 'fa fa-scissors', hotkey: 'Ctrl+X'},
				{uno: '.uno:Copy', icon: 'fa fa-files-o', hotkey: 'Ctrl+C'},
				{uno: '.uno:Paste', icon: 'fa fa-clipboard', hotkey: 'Ctrl+V'},
				{uno: '.uno:SelectAll', icon: 'fa fa-check-circle', hotkey: 'Ctrl+A'},
				{type: 'separator'},
				{uno: '.uno:SearchDialog'},
				{type: 'separator'},
				{name: _UNO('.uno:ChangesMenu', 'text'), id: 'changesmenu', type: 'menu', menu: [
					{uno: '.uno:TrackChanges'},
					{uno: '.uno:ShowTrackedChanges'},
					{type: 'separator'},
					{uno: '.uno:AcceptTrackedChanges'},
					{uno: '.uno:AcceptAllTrackedChanges'},
					{uno: '.uno:RejectAllTrackedChanges'},
					{uno: '.uno:PreviousTrackedChange'},
					{uno: '.uno:NextTrackedChange'}
				]}/*,
				{uno: '.uno:EditStyle'}*/
			]},
			{name: _UNO('.uno:ViewMenu', 'text'), id: 'view', type: 'menu', menu: [
				{name: _UNO('.uno:FullScreen', 'text'), id: 'fullscreen', type: 'action', icon: 'fa fa-arrows-alt'},
				{type: 'separator'},
				{name: _UNO('.uno:ZoomPlus', 'text'), id: 'zoomin', type: 'action', icon: 'fa fa-search-plus'},
				{name: _UNO('.uno:ZoomMinus', 'text'), id: 'zoomout', type: 'action', icon: 'fa fa-search-minus'},
				{name: _('Reset zoom'), id: 'zoomreset', type: 'action', icon: 'fa fa-search'},
				{type: 'separator'},
				{uno: '.uno:ControlCodes', hotkey: 'Ctrl+F10'}
			]
			},
			{name: _UNO('.uno:InsertMenu', 'text'), type: 'menu', menu: [
				{name: _('Local Image...'), id: 'insertgraphic', type: 'action', icon: 'fa fa-file-image-o'},
				{name: _UNO('.uno:InsertGraphic', 'text'), id: 'insertgraphicremote', type: 'action', icon: 'fa fa-file-image-o'},
				{name: _UNO('.uno:InsertAnnotation', 'text'), id: 'insertcomment', type: 'action', icon: 'fa fa-comment-o', hotkey: 'Ctrl+Alt+C'},
				{uno: '.uno:InsertObjectChart', icon: 'fa fa-bar-chart'},
				{type: 'separator'},
				{name: _UNO('.uno:InsertField', 'text'), type: 'menu', menu: [
					{uno: '.uno:InsertPageNumberField'},
					{uno: '.uno:InsertPageCountField'},
					{uno: '.uno:InsertDateField'},
					{uno: '.uno:InsertTimeField'},
					{uno: '.uno:InsertTitleField'},
					{uno: '.uno:InsertAuthorField'},
					{uno: '.uno:InsertTopicField'}
				]},
				{name: _UNO('.uno:InsertHeaderFooterMenu', 'text'), type: 'menu', menu: [
					{name: _UNO('.uno:InsertPageHeader', 'text'), type: 'menu', menu: [
						{name: _('All'), disabled: true, id: 'insertheader', tag: '_ALL_', uno: '.uno:InsertPageHeader?'}]},
					{name: _UNO('.uno:InsertPageFooter', 'text'), type: 'menu', menu: [
						{name: _('All'), disabled: true, id: 'insertfooter', tag: '_ALL_', uno: '.uno:InsertPageFooter?'}]}
				]},
				{uno: '.uno:InsertFootnote', hotkey: 'Ctrl+Alt+F'},
				{uno: '.uno:InsertEndnote', hotkey: 'Ctrl+Alt+D'},
				{type: 'separator'},
				{uno: '.uno:InsertPagebreak', icon: 'fa fa-wpforms', hotkey: 'Ctrl+Enter'},
				{uno: '.uno:InsertColumnBreak', icon: 'fa fa-columns', hotkey: 'Ctrl+Shift+Enter'},
				{type: 'separator'},
				{uno: '.uno:HyperlinkDialog', icon: 'fa fa-link', hotkey: 'Ctrl+K'},
				{uno: '.uno:InsertBookmark', icon: 'fa fa-bookmark'},
				{uno: '.uno:InsertSymbol', icon: 'fa fa-wikipedia-w'},
				{name: _('Horizontal Line'), id: 'inserthorizontalline', type: 'action'},
				{name: _UNO('.uno:FormattingMarkMenu', 'text'), type: 'menu', menu: [
					{uno: '.uno:InsertNonBreakingSpace'},
					{uno: '.uno:InsertHardHyphen'},
					{uno: '.uno:InsertSoftHyphen'},
					{uno: '.uno:InsertZWSP'},
					{uno: '.uno:InsertZWNBSP'},
					{uno: '.uno:InsertLRM'},
					{uno: '.uno:InsertRLM'}]}/*,
				{uno: '.uno:InsertIndexesEntry'}*/
			]
			},
			{name: _UNO('.uno:FormatMenu', 'text'), type: 'menu', menu: [
				{name: _UNO('.uno:FormatTextMenu', 'text'), icon:'fa fa-font', type: 'menu', menu: [
					{uno: '.uno:Bold', icon: 'fa fa-bold', hotkey: 'Ctrl+B'},
					{uno: '.uno:Italic', icon: 'fa fa-italic', hotkey: 'Ctrl+I'},
					{uno: '.uno:Underline', icon: 'fa fa-underline', hotkey: 'Ctrl+U'},
					{uno: '.uno:UnderlineDouble', icon: 'fa fa-underline', hotkey: 'Ctrl+D'},
					{uno: '.uno:Strikeout', icon: 'fa fa-strikethrough', hotkey: 'Ctrl+Alt+5'},
					{uno: '.uno:Overline'},
					{type: 'separator'},
					{uno: '.uno:SuperScript', icon: 'fa fa-superscript', hotkey: 'Ctrl+Shift+P'},
					{uno: '.uno:SubScript', icon: 'fa fa-subscript', hotkey: 'Ctrl+Shift+B'},
					{uno: '.uno:SmallCaps'},
					{type: 'separator'},
					{uno: '.uno:Shadowed'},
					{uno: '.uno:OutlineFont'},
					{type: 'separator'},
					{uno: '.uno:Grow'},
					{uno: '.uno:Shrink'},
					{type: 'separator'},
					{uno: '.uno:ChangeCaseToUpper'},
					{uno: '.uno:ChangeCaseToLower'},
					{uno: '.uno:ChangeCaseRotateCase'},
					{type: 'separator'},
					{uno: '.uno:ChangeCaseToSentenceCase'},
					{uno: '.uno:ChangeCaseToTitleCase'},
					{uno: '.uno:ChangeCaseToToggleCase'}]},
				{name: _('Text orientation'), type: 'menu', menu: [
					{uno: '.uno:ParaLeftToRight'},
					{uno: '.uno:ParaRightToLeft'}]},
				{name: _UNO('.uno:FormatSpacingMenu', 'text'), icon: 'fa fa-text-height', type: 'menu', menu: [
					{uno: '.uno:SpacePara1'},
					{uno: '.uno:SpacePara15'},
					{uno: '.uno:SpacePara2'},
					{type: 'separator'},
					{uno: '.uno:ParaspaceIncrease'},
					{uno: '.uno:ParaspaceDecrease'},
					{type: 'separator'},
					{uno: '.uno:IncrementIndent', icon: 'fa fa-indent'},
					{uno: '.uno:DecrementIndent', icon: 'fa fa-outdent'}]},
				{name: _UNO('.uno:TextAlign', 'text'), icon: 'fa fa-align-justify', type: 'menu', menu: [
					{uno: '.uno:CommonAlignLeft', icon: 'fa fa-align-left', hotkey: 'Ctrl+L'},
					{uno: '.uno:CommonAlignHorizontalCenter', icon: 'fa fa-align-center', hotkey: 'Ctrl+E'},
					{uno: '.uno:CommonAlignRight', icon: 'fa fa-align-right', hotkey: 'Ctrl+R'},
					{uno: '.uno:CommonAlignJustified', icon: 'fa fa-align-justify', hotkey: 'Ctrl+J'},
					{type: 'separator'},
					{uno: '.uno:CommonAlignTop', icon: 'fa fa-chevron-up'},
					{uno: '.uno:CommonAlignVerticalCenter'},
					{uno: '.uno:CommonAlignBottom', icon: 'fa fa-chevron-down'}]},
				{name: _UNO('.uno:FormatBulletsMenu', 'text'), icon: 'fa fa-list', type: 'menu', menu: [
					{uno: '.uno:DefaultBullet', icon: 'fa fa-list-ul'},
					{uno: '.uno:DefaultNumbering', icon: 'fa fa-list-ol'},
					{type: 'separator'},
					{uno: '.uno:DecrementLevel', icon: 'fa fa-indent'},
					{uno: '.uno:IncrementLevel', icon: 'fa fa-outdent'},
					{uno: '.uno:DecrementSubLevels'},
					{uno: '.uno:IncrementSubLevels'},
					{type: 'separator'},
					{uno: '.uno:MoveDown'},
					{uno: '.uno:MoveUp'},
					{uno: '.uno:MoveDownSubItems'},
					{uno: '.uno:MoveUpSubItems'},
					{type: 'separator'},
					{uno: '.uno:InsertNeutralParagraph'},
					{uno: '.uno:NumberingStart'},
					{type: 'separator'},
					{uno: '.uno:JumpDownThisLevel'},
					{uno: '.uno:JumpUpThisLevel'},
					{uno: '.uno:ContinueNumbering'}]},
				{type: 'separator'},
				{uno: '.uno:FontDialog'},
				{uno: '.uno:ParagraphDialog', icon: 'fa fa-paragraph'},
				{uno: '.uno:OutlineBullet'},
				{uno: '.uno:PageDialog', icon: 'fa fa-file-o'},
				{type: 'separator'},
				{uno: '.uno:FormatColumns', icon: 'fa fa-columns'},
				{type: 'separator'},
				{uno: '.uno:ResetAttributes', icon: 'fa fa-eraser', hotkey: 'Ctrl+M'},
				{type: 'separator'},
				{name: _('Page size'), type: 'menu', menu: [
					{name: 'A4, ' + _('Portrait'), id:'a4portrait', type: 'action'},
					{name: 'A4, ' + _('Landscape'), id:'a4landscape', type: 'action'},
					{name: 'A5, ' + _('Portrait'), id:'a5portrait', type: 'action'},
					{name: 'A5, ' + _('Landscape'), id:'a5landscape', type: 'action'},
					{name: 'Letter, ' + _('Portrait'), id:'letterportrait', type: 'action'},
					{name: 'Letter, ' + _('Landscape'), id:'letterlandscape', type: 'action'},
					{name: 'Legal, ' + _('Portrait'), id:'legalportrait', type: 'action'},
					{name: 'Legal, ' + _('Landscape'), id:'legallandscape', type: 'action'}
				]}
			]},
			{name: _UNO('.uno:TableMenu', 'text'), type: 'menu', menu: [
				{name: _UNO('.uno:TableInsertMenu', 'text'), type: 'menu', menu: [
					{uno: '.uno:InsertRowsBefore'},
					{uno: '.uno:InsertRowsAfter'},
					{type: 'separator'},
					{uno: '.uno:InsertColumnsBefore'},
					{uno: '.uno:InsertColumnsAfter'}]},
				{name: _UNO('.uno:TableDeleteMenu', 'text'), type: 'menu', menu: [
					{uno: '.uno:DeleteRows'},
					{uno: '.uno:DeleteColumns'},
					{uno: '.uno:DeleteTable'}]},
				{name: _UNO('.uno:TableSelectMenu', 'text'), type: 'menu', menu: [
					{uno: '.uno:SelectTable', icon: 'fa fa-table'},
					{uno: '.uno:EntireRow'},
					{uno: '.uno:EntireColumn'},
					{uno: '.uno:EntireCell'}]},
				{uno: '.uno:MergeCells'}/*,
				{type: 'separator'},
				{uno: '.uno:TableDialog'}*/
			]},
			{name: _UNO('.uno:ToolsMenu', 'text'), id: 'tools', type: 'menu', menu: [
				{uno: '.uno:SpellingAndGrammarDialog'},
				{uno: '.uno:SpellOnline'},
				//{uno: '.uno:ThesaurusDialog'},
				{name: _UNO('.uno:LanguageMenu'), icon: 'fa fa-language', type: 'menu', menu: [
					{name: _UNO('.uno:SetLanguageSelectionMenu', 'text'), type: 'menu', menu: [
						{name: _('None (Do not check spelling)'), id: 'noneselection', uno: '.uno:LanguageStatus?Language:string=Current_LANGUAGE_NONE'}]},
					{name: _UNO('.uno:SetLanguageParagraphMenu', 'text'), type: 'menu', menu: [
						{name: _('None (Do not check spelling)'), id: 'noneparagraph', uno: '.uno:LanguageStatus?Language:string=Paragraph_LANGUAGE_NONE'}]},
					{name: _UNO('.uno:SetLanguageAllTextMenu', 'text'), type: 'menu', menu: [
						{name: _('None (Do not check spelling)'), id: 'nonelanguage', uno: '.uno:LanguageStatus?Language:string=Default_LANGUAGE_NONE'}]}
				]},
				{uno: '.uno:WordCountDialog'},
				{type: 'separator'},
				{name: _UNO('.uno:AutoFormatMenu', 'text'), type: 'menu', menu: [
					{uno: '.uno:OnlineAutoFormat'}]}
			]},
			{name: _UNO('.uno:HelpMenu', 'text'), id: 'help', type: 'menu', menu: [
				{name: _('Keyboard shortcuts'), id: 'keyboard-shortcuts', type: 'action', icon: 'fa fa-keyboard-o', hotkey: 'Ctel+Shift+?'},
				{name: _('About'), id: 'about', type: 'action'}]
			},
			{name: _('Last modification'), id: 'last-mod', type: 'action', mobile: false, tablet: false}
		],

		presentation: [
			{name: _UNO('.uno:PickList', 'presentation'), id: 'file', type: 'menu', menu: [
				{name: _UNO('.uno:Save', 'presentation'), id: 'save', type: 'action', icon:'fa fa-floppy-o', hotkey: 'Ctrl+S'},
				{name: _UNO('.uno:SaveAs', 'presentation'), id: 'saveas', type: 'action', icon:'fa fa-download'},
				{name: _('Share...'), id:'shareas', type: 'action', icon: 'fa fa-share-alt'},
				{name: _UNO('.uno:Print', 'presentation'), id: 'print', type: 'action', icon: 'fa fa-print', hotkey: 'Ctrl+P'},
				{name: _('See revision history'), id: 'rev-history', type: 'action'},
				{name: _('Download as'), id: 'downloadas', icon:'fa fa-cloud-download', type: 'menu', menu: [
					{name: _('PDF Document (.pdf)'), id: 'downloadas-pdf', type: 'action', icon:'fa fa-file-pdf-o'},
					{name: _('HTML Document (.html)'), id: 'downloadas-html', icon: 'fa fa-file-pdf-o', type: 'action'},
					{name: _('ODF presentation (.odp)'), id: 'downloadas-odp', type: 'action', icon: 'fa fa-file-text-o'},
					{name: _('PowerPoint 2003 Presentation (.ppt)'), id: 'downloadas-ppt', type: 'action', icon: 'fa fa-file-powerpoint-o'},
					{name: _('PowerPoint Presentation (.pptx)'), id: 'downloadas-pptx', type: 'action', icon: 'fa fa-file-powerpoint-o'}]},
				/*{type: 'separator'},
				{uno: '.uno:SetDocumentProperties'},*/
				{type: 'separator'},
				{name: _UNO('CloseDoc'), id: 'closedocument', type: 'action', icon:'fa fa-times-circle'}
			]},
			{name: _UNO('.uno:EditMenu', 'presentation'), type: 'menu', menu: [
				{uno: '.uno:Undo', icon: 'fa fa-undo', hotkey: 'Ctrl+Z'},
				{uno: '.uno:Redo', icon: 'fa fa-repeat', hotkey: 'Ctrl+Y'},
				{name: _('Repair'), id: 'repair',  type: 'action'},
				{type: 'separator'},
				{uno: '.uno:Cut', icon: 'fa fa-scissors', hotkey: 'Ctrl+X'},
				{uno: '.uno:Copy', icon: 'fa fa-files-o', hotkey: 'Ctrl+C'},
				{uno: '.uno:Paste', icon: 'fa fa-clipboard', hotkey: 'Ctrl+V'},
				{uno: '.uno:SelectAll', icon: 'fa fa-check-circle', hotkey: 'Ctrl+A'},
				{type: 'separator'},
				{uno: '.uno:SearchDialog'}
			]},
			{name: _UNO('.uno:ViewMenu', 'presentation'), id: 'view', type: 'menu', menu: [
				{name: _UNO('.uno:FullScreen', 'presentation'), id: 'fullscreen', type: 'action', icon: 'fa fa-arrows-alt'},
				{type: 'separator'},
				{name: _UNO('.uno:ZoomPlus', 'presentation'), id: 'zoomin', type: 'action', icon: 'fa fa-search-plus'},
				{name: _UNO('.uno:ZoomMinus', 'presentation'), id: 'zoomout', type: 'action', icon: 'fa fa-search-minus'},
				{name: _('Reset zoom'), id: 'zoomreset', type: 'action', icon: 'fa fa-search'}]
			},
			{name: _UNO('.uno:InsertMenu', 'presentation'), type: 'menu', menu: [
				{name: _('Local Image...'), id: 'insertgraphic', type: 'action', icon: 'fa fa-file-image-o'},
				{name: _UNO('.uno:InsertGraphic', 'presentation'), id: 'insertgraphicremote', type: 'action', icon: 'fa fa-file-image-o'},
				{name: _UNO('.uno:InsertAnnotation', 'presentation'), id: 'insertcomment', type: 'action', icon: 'fa fa-commenting-o', hotkey: 'Ctrl+Alt+C'},
				{uno: '.uno:InsertObjectChart', icon: 'fa fa-bar-chart'},
				{type: 'separator'},
				{uno: '.uno:HyperlinkDialog', icon: 'fa fa-link', hotkey: 'Ctrl+K'},
				{type: 'separator'},
				{uno: '.uno:InsertSymbol', icon: 'fa fa-wikipedia-w'},
				{type: 'separator'},
				{name: _UNO('.uno:HeaderAndFooter', 'presentation'), uno: '.uno:HeaderAndFooter'}]
			},
			{name: _UNO('.uno:FormatMenu', 'presentation'), type: 'menu', menu: [
				{uno: '.uno:FontDialog', icon: 'fa fa-font'},
				{uno: '.uno:ParagraphDialog', icon: 'fa fa-paragraph'},
				{uno: '.uno:PageSetup', icon: 'fa fa-file-o'},
				{type: 'separator'},
				{uno: '.uno:TransformDialog'},
				{uno: '.uno:FormatLine'},
				{uno: '.uno:FormatArea'},
				{type: 'separator'},
				{uno: '.uno:OutlineBullet', icon: 'fa fa-list'}]
			},
			{uno: '.uno:TableMenu', type: 'menu', menu: [
				{uno: '.uno:TableInsertMenu', type: 'menu', menu: [
					{uno: '.uno:InsertRowsBefore'},
					{uno: '.uno:InsertRowsAfter'},
					{type: 'separator'},
					{uno: '.uno:InsertColumnsBefore'},
					{uno: '.uno:InsertColumnsAfter'}]},
				{uno: '.uno:TableDeleteMenu', type: 'menu', menu: [
					{uno: '.uno:DeleteRows'},
					{uno: '.uno:DeleteColumns'}]},
				{uno: '.uno:MergeCells'}]
			},
			{name: _UNO('.uno:SlideMenu', 'presentation'), type: 'menu', menu: [
				{name: _UNO('.uno:InsertSlide', 'presentation'), id: 'insertpage', icon: 'img-icon insertpage',type: 'action'},
				{name: _UNO('.uno:DuplicateSlide', 'presentation'), id: 'duplicatepage', icon: 'img-icon duplicatepage', type: 'action'},
				{name: _UNO('.uno:DeleteSlide', 'presentation'), id: 'deletepage', icon: 'img-icon deletepage', type: 'action'},
				{type: 'separator', id: 'fullscreen-presentation-separator'},
				{name: _('Fullscreen presentation'), id: 'fullscreen-presentation', icon: 'img-icon presentation', type: 'action'}]
			},
			{name: _UNO('.uno:ToolsMenu', 'presentation'), id: 'tools', type: 'menu', menu: [
				{uno: '.uno:SpellOnline'},
				{name: _UNO('.uno:LanguageMenu'), icon: 'fa fa-language', type: 'menu', menu: [
					{name: _('None (Do not check spelling)'), id: 'nonelanguage', uno: '.uno:LanguageStatus?Language:string=Default_LANGUAGE_NONE'}]}
			]},
			{name: _UNO('.uno:HelpMenu', 'presentation'), id: 'help', type: 'menu', menu: [
				{name: _('Keyboard shortcuts'), id: 'keyboard-shortcuts', type: 'action', icon: 'fa fa-keyboard-o', hotkey: 'Ctrl+Shift+?'},
				{name: _('About'), id: 'about', type: 'action'}]
			},
			{name: _('Last modification'), id: 'last-mod', type: 'action', mobile: false, tablet: false}
		],

		spreadsheet: [
			{name: _UNO('.uno:PickList', 'spreadsheet'), id: 'file', type: 'menu', menu: [
				{name: _UNO('.uno:Save', 'spreadsheet'), id: 'save', type: 'action', icon:'fa fa-floppy-o', hotkey: 'Ctrl+S'},
				{name: _UNO('.uno:SaveAs', 'spreadsheet'), id: 'saveas', type: 'action', icon:'fa fa-floppy-o', hotkey: 'Ctrl+S'},
				{name: _('Share...'), id:'shareas', type: 'action', icon: 'fa fa-share-alt'},
				{name: _UNO('.uno:Print', 'spreadsheet'), id: 'print', type: 'action', icon: 'fa fa-print', hotkey: 'Ctrl+P'},
				{name: _('See revision history'), id: 'rev-history', type: 'action', icon:'fa fa-history'},
				{name: _('Download as'), id:'downloadas', icon:'fa fa-cloud-download', type: 'menu', menu: [
					{name: _('HTML Document (.html)'), id: 'downloadas-html', icon: 'fa fa-file-pdf-o', type: 'action'},
					{name: _('PDF Document (.pdf)'), id: 'downloadas-pdf', type: 'action', icon: 'fa fa-file-pdf-o'},
					{name: _('ODF spreadsheet (.ods)'), id: 'downloadas-ods', type: 'action', icon: 'fa fa-file-text-o'},
					{name: _('Excel 2003 Spreadsheet (.xls)'), id: 'downloadas-xls', type: 'action', icon:'fa fa-file-excel-o'},
					{name: _('Excel Spreadsheet (.xlsx)'), id: 'downloadas-xlsx', type: 'action', icon:'fa fa-file-excel-o'},
					{name: _('CSV (.csv)'), id: 'downloadas-csv', icon:'fa fa-file-excel-o', type: 'action'}]},
				/*{type: 'separator'},
				{uno: '.uno:SetDocumentProperties'},*/
				{type: 'separator'},
				{name: _UNO('CloseDoc'), id: 'closedocument', type: 'action', icon:'fa fa-times-circle'}
			]},
			{name: _UNO('.uno:EditMenu', 'spreadsheet'), type: 'menu', menu: [
				{uno: '.uno:Undo', icon: 'fa fa-undo', hotkey: 'Ctrl+Z'},
				{uno: '.uno:Redo', icon: 'fa fa-repeat', hotkey: 'Ctrl+Y'},
				{name: _('Repair'), id: 'repair',  type: 'action'},
				{type: 'separator'},
				{uno: '.uno:Cut', icon: 'fa fa-scissors', hotkey: 'Ctrl+X'},
				{uno: '.uno:Copy', icon: 'fa fa-files-o', hotkey: 'Ctrl+C'},
				{uno: '.uno:Paste', icon: 'fa fa-clipboard', hotkey: 'Ctrl+V'},
				{uno: '.uno:SelectAll'},
				{type: 'separator'},
				{uno: '.uno:SearchDialog'}
			]},
			{name: _UNO('.uno:ViewMenu', 'spreadsheet'), id: 'view', type: 'menu', menu: [
				{name: _UNO('.uno:FullScreen', 'spreadsheet'), id: 'fullscreen', type: 'action', icon: 'fa fa-arrows-alt'},
				{type: 'separator'},
				{name: _UNO('.uno:ZoomPlus', 'spreadsheet'), id: 'zoomin', type: 'action', icon: 'fa fa-search-plus'},
				{name: _UNO('.uno:ZoomMinus', 'spreadsheet'), id: 'zoomout', type: 'action', icon: 'fa fa-search-minus'},
				{name: _('Reset zoom'), id: 'zoomreset', type: 'action', icon: 'fa fa-search'}
			]},
			{name: _UNO('.uno:InsertMenu', 'spreadsheet'), type: 'menu', menu: [
				{name: _('Local Image...'), id: 'insertgraphic', type: 'action', icon: 'fa fa-picture-o'},
				{name: _UNO('.uno:InsertGraphic', 'spreadsheet'), id: 'insertgraphicremote', type: 'action', icon: 'fa fa-picture-o'},
				{uno: '.uno:InsertObjectChart', icon: 'fa fa-bar-chart'},
				{name: _UNO('.uno:InsertAnnotation', 'spreadsheet'), id: 'insertcomment', type: 'action', icon: 'fa fa-commenting-o', hotkey: 'Ctrl+Alt+C'},
				{type: 'separator'},
				{uno: '.uno:HyperlinkDialog', icon: 'fa fa-link', hotkey: 'Ctrk+K'},
				{uno: '.uno:InsertSymbol', icon: 'fa fa-wikipedia-w'},
				{uno: '.uno:EditHeaderAndFooter'}
			]},
			{name: _UNO('.uno:FormatMenu', 'spreadsheet'), type: 'menu', menu: [
				{uno: '.uno:ResetAttributes', icon: 'fa fa-eraser', hotkey: 'Ctrl+M'},
				{uno: '.uno:FormatCellDialog'},
				{uno: '.uno:PageFormatDialog', icon: 'fa fa-file-o'}
			]},
			{name: _UNO('.uno:SheetMenu', 'spreadsheet'), type: 'menu', menu: [
				{name: _UNO('.uno:InsertRowsMenu', 'spreadsheet'), type: 'menu', menu: [
					{uno: '.uno:InsertRowsBefore'},
					{uno: '.uno:InsertRowsAfter'}]},
				{name: _UNO('.uno:InsertColumnsMenu', 'spreadsheet'), type: 'menu', menu: [
					{uno: '.uno:InsertColumnsBefore'},
					{uno: '.uno:InsertColumnsAfter'}]},
				{name: _UNO('.uno:InsertBreakMenu', 'spreadsheet'), type: 'menu', menu: [
					{uno: '.uno:InsertRowBreak'},
					{uno: '.uno:InsertColumnBreak'}]},
				{type: 'separator'},
				{uno: '.uno:DeleteRows'},
				{uno: '.uno:DeleteColumns'},
				{name: _UNO('.uno:DelBreakMenu', 'spreadsheet'), type: 'menu', menu: [
					{uno: '.uno:DeleteRowbreak'},
					{uno: '.uno:DeleteColumnbreak'}]}
			]},
			{name: _UNO('.uno:DataMenu', 'spreadsheet'), type: 'menu', menu: [
				{uno: '.uno:DataSort'},
				{uno: '.uno:SortAscending', icon: 'fa fa-sort-alpha-asc'},
				{uno: '.uno:SortDescending', icon: 'fa fa-sort-alpha-desc'},
				{uno: '.uno:Validation'},
				{type: 'separator'},
				{uno: '.uno:DataFilterAutoFilter', icon: 'fa fa-filter'},
				{name: _UNO('.uno:FilterMenu', 'spreadsheet'), type: 'menu', menu: [
					{uno: '.uno:DataFilterStandardFilter'},
					{uno: '.uno:DataFilterSpecialFilter'},
					{type: 'separator'},
					{uno: '.uno:DataFilterRemoveFilter'},
					{uno: '.uno:DataFilterHideAutoFilter'}]},
				{type: 'separator'},
				{name: _UNO('.uno:GroupOutlineMenu', 'spreadsheet'), type: 'menu', menu: [
					{uno: '.uno:Group'},
					{uno: '.uno:Ungroup'},
					{type: 'separator'},
					{uno: '.uno:ClearOutline'},
					{type: 'separator'},
					{uno: '.uno:HideDetail'},
					{uno: '.uno:ShowDetail'}]}
			]},
			{name: _UNO('.uno:ToolsMenu', 'spreadsheet'), id: 'tools', type: 'menu', menu: [
				{uno: '.uno:SpellOnline'},
				{name: _UNO('.uno:LanguageMenu'), icon: 'fa fa-language', type: 'menu', menu: [
					{name: _('None (Do not check spelling)'), id: 'nonelanguage', uno: '.uno:LanguageStatus?Language:string=Default_LANGUAGE_NONE'}]}
			]},
			{name: _UNO('.uno:HelpMenu', 'spreadsheet'), id: 'help', type: 'menu', menu: [
				{name: _('Keyboard shortcuts'), id: 'keyboard-shortcuts', type: 'action', icon: 'fa fa-keyboard-o', hotkey: 'Ctrl+Shift+?'},
				{name: _('About'), id: 'about', type: 'action'}]
			},
			{name: _('Last modification'), id: 'last-mod', type: 'action', mobile: false, tablet: false}
		],

		commandStates: {},

		// Only these menu options will be visible in readonly mode
		allowedReadonlyMenus: ['file', 'downloadas', 'view', 'help'],

		allowedViewModeActions: [
			'downloadas-pdf', 'downloadas-odt', 'downloadas-doc', 'downloadas-docx', 'downloadas-rtf', // file menu
			'downloadas-odp', 'downloadas-ppt', 'downloadas-pptx', 'print', // file menu
			'downloadas-ods', 'downloadas-xls', 'downloadas-xlsx', 'closedocument', // file menu
			'downloadas-csv', 'downloadas-html', 'downloadas-txt',  // file menu
			'fullscreen', 'zoomin', 'zoomout', 'zoomreset', // view menu
			'about', 'keyboard-shortcuts' // help menu
		]
	},

	onAdd: function (map) {
		this._initialized = false;
		this._menubarCont = L.DomUtil.get('main-menu');
		this._initializeMenu(this.options.initial);

		map.on('doclayerinit', this._onDocLayerInit, this);
		map.on('updatepermission', this._onRefresh, this);
		map.on('addmenu', this._addMenu, this);
		map.on('commandvalues', this._onInitMenu, this);
		map.on('updatetoolbarcommandvalues', this._onStyleMenu, this);
	},

	_addMenu: function (e) {
		var alreadyExists = L.DomUtil.get('menu-' + e.id);
		if (alreadyExists)
			return;

		var liItem = L.DomUtil.create('li', '');
		liItem.id = 'menu-' + e.id;
		if (this._map._permission === 'readonly') {
			L.DomUtil.addClass(liItem, 'readonly');
		}
		var aItem = L.DomUtil.create('a', '', liItem);
		$(aItem).text(e.label);
		$(aItem).data('id', e.id);
		$(aItem).data('type', 'action');
		$(aItem).data('postmessage', 'true');
		this._menubarCont.insertBefore(liItem, this._menubarCont.firstChild);
	},

	_createUnoMenuItem: function (caption, command, tag) {
		var liItem, aItem;
		liItem = L.DomUtil.create('li', '');
		aItem = L.DomUtil.create('a', '', liItem);
		$(aItem).text(caption);
		$(aItem).data('type', 'unocommand');
		$(aItem).data('uno', command);
		$(aItem).data('tag', tag);
		return liItem;
	},


	_onInitMenu: function (e) {
		if (e.commandName === '.uno:LanguageStatus' && L.Util.isArray(e.commandValues)) {
			var translated, neutral;
			var constDefa = 'Default_RESET_LANGUAGES';
			var constCurr = 'Current_RESET_LANGUAGES';
			var constPara = 'Paragraph_RESET_LANGUAGES';
			var constLang = '.uno:LanguageStatus?Language:string=';
			var resetLang = _('Reset to Default Language');
			var languages  = [];

			e.commandValues.forEach(function(language) {
				languages.push({translated: _(language), neutral: language});
			});
			languages.sort(function(a, b) {
				return a.translated < b.translated ? -1 : a.translated > b.translated ? 1 : 0;
			});

			var $menuSelection = $('#menu-noneselection').parent();
			var $menuParagraph = $('#menu-noneparagraph').parent();
			var $menuDefault = $('#menu-nonelanguage').parent();
			for (var lang in languages) {
				translated = languages[lang].translated;
				neutral = languages[lang].neutral;
				$menuSelection.append(this._createUnoMenuItem(translated, constLang + encodeURIComponent('Current_' + neutral)));
				$menuParagraph.append(this._createUnoMenuItem(translated, constLang + encodeURIComponent('Paragraph_' + neutral)));
				$menuDefault.append(this._createUnoMenuItem(translated, constLang + encodeURIComponent('Default_' + neutral)));
			}
			$menuSelection.append(this._createMenu([{type: 'separator'}]));
			$menuParagraph.append(this._createMenu([{type: 'separator'}]));
			$menuDefault.append(this._createMenu([{type: 'separator'}]));
			$menuSelection.append(this._createUnoMenuItem(resetLang, constLang + constCurr));
			$menuParagraph.append(this._createUnoMenuItem(resetLang, constLang + constPara));
			$menuDefault.append(this._createUnoMenuItem(resetLang, constLang + constDefa));
		}
	},

	_onRefresh: function() {
		// clear initial menu
		while (this._menubarCont.hasChildNodes()) {
			this._menubarCont.removeChild(this._menubarCont.firstChild);
		}

		// Add document specific menu
		var docType = this._map.getDocType();
		if (docType === 'text') {
			this._initializeMenu(this.options.text);
		} else if (docType === 'spreadsheet') {
			this._initializeMenu(this.options.spreadsheet);
		} else if (docType === 'presentation' || docType === 'drawing') {
			this._initializeMenu(this.options.presentation);
		}

		// initialize menubar plugin
		$('#main-menu').smartmenus({
			hideOnClick: true,
			showOnClick: true,
			hideTimeout: 0,
			hideDuration: 0,
			showDuration: 0,
			showTimeout: 0,
			collapsibleHideDuration: 0,
			subIndicatorsPos: 'append',
			subIndicatorsText: '&#8250;'
		});
		$('#main-menu').attr('tabindex', 0);

		if (this._map._permission !== 'readonly') {
			this._createFileIcon();
		}
	},

	_onStyleMenu: function (e) {
		if (e.commandName === '.uno:StyleApply') {
			var style;
			var constArg = '&';
			var constHeader = '.uno:InsertPageHeader?PageStyle:string=';
			var constFooter = '.uno:InsertPageFooter?PageStyle:string=';
			var $menuHeader = $('#menu-insertheader').parent();
			var $menuFooter = $('#menu-insertfooter').parent();
			var pageStyles = e.commandValues['HeaderFooter'];
			for (var iterator in pageStyles) {
				style = pageStyles[iterator];
				$menuHeader.append(this._createUnoMenuItem(_(style), constHeader + encodeURIComponent(style) + constArg, style));
				$menuFooter.append(this._createUnoMenuItem(_(style), constFooter + encodeURIComponent(style) + constArg, style));
			}
		}
	},

	_createDocument: function(e) {
		var self = e.data.self;
		var docType = self._map.getDocType();
		self._map.fire('postMessage', {msgId: 'UI_CreateFile', args: {DocumentType: docType}});
	},

	_onDocLayerInit: function() {
		this._onRefresh();

		$('#main-menu').bind('select.smapi', {self: this}, this._onItemSelected);
		$('#main-menu').bind('mouseenter.smapi', {self: this}, this._onMouseEnter);
		$('#main-menu').bind('mouseleave.smapi', {self: this}, this._onMouseLeave);

		$('#main-menu').bind('beforeshow.smapi', {self: this}, this._beforeShow);
		$('#main-menu').bind('click.smapi', {self: this}, this._onClicked);

		$('#main-menu').bind('keydown', {self: this}, this._onKeyDown);

		// SmartMenus mobile menu toggle button
		$(function() {
			var $mainMenuState = $('#main-menu-state');
			if ($mainMenuState.length) {
				// animate mobile menu
				$mainMenuState.change(function() {
					var $menu = $('#main-menu');
					var $nav = $menu.parent();
					if (this.checked) {
						$nav.css({height: 'initial', bottom: '38px'});
						$menu.hide().slideDown(250, function() { $menu.css('display', ''); });
					} else {
						$menu.show().slideUp(250, function() { $menu.css('display', ''); });
						$nav.css({height:'', bottom: ''});
					}
				});
				// hide mobile menu beforeunload
				$(window).bind('beforeunload unload', function() {
					if ($mainMenuState[0].checked) {
						$mainMenuState[0].click();
					}
				});
			}
		});

		this._initialized = true;
	},

	_onClicked: function(e, menu) {
		if ($(menu).hasClass('highlighted')) {
			$('#main-menu').smartmenus('menuHideAll');
		}

		var $mainMenuState = $('#main-menu-state');
		if (!$(menu).hasClass('has-submenu') && $mainMenuState[0].checked) {
			$mainMenuState[0].click();
		}
	},

	_checkedMenu: function(uno, item) {
		var constChecked = 'lo-menu-item-checked';
		var state = this._map['stateChangeHandler'].getItemValue(uno);
		var data = $(item).data('tag');
		state = state[data] || false;
		if (state) {
			$(item).addClass(constChecked);
		} else {
			$(item).removeClass(constChecked);
		}
	},

	_beforeShow: function(e, menu) {
		var self = e.data.self;
		var items = $(menu).children().children('a').not('.has-submenu');
		$(items).each(function() {
			var aItem = this;
			var type = $(aItem).data('type');
			var id = $(aItem).data('id');
			if (self._map._permission === 'edit') {
				if (type === 'unocommand') { // enable all depending on stored commandStates
					var data, lang;
					var constUno = 'uno';
					var constState = 'stateChangeHandler';
					var constChecked = 'lo-menu-item-checked';
					var constLanguage = '.uno:LanguageStatus';
					var constPageHeader = '.uno:InsertPageHeader';
					var constPageFooter = '.uno:InsertPageFooter';
					var unoCommand = $(aItem).data(constUno);
					var itemState = self._map[constState].getItemValue(unoCommand);
					if (itemState === 'disabled') {
						$(aItem).addClass('disabled');
					} else {
						$(aItem).removeClass('disabled');
					}
					if (unoCommand.startsWith(constLanguage)) {
						unoCommand = constLanguage;
						lang = self._map[constState].getItemValue(unoCommand);
						data = decodeURIComponent($(aItem).data(constUno));
						if (data.indexOf(lang) !== -1) {
							$(aItem).addClass(constChecked);
						} else if (data.indexOf('LANGUAGE_NONE') !== -1 && lang === '[None]') {
							$(aItem).addClass(constChecked);
						} else {
							$(aItem).removeClass(constChecked);
						}
					}
					else if (unoCommand.startsWith(constPageHeader)) {
						unoCommand = constPageHeader;
						self._checkedMenu(unoCommand, this);
					}
					else if (unoCommand.startsWith(constPageFooter)) {
						unoCommand = constPageFooter;
						self._checkedMenu(unoCommand, this);
					}
					else if (itemState === 'true') {
						$(aItem).addClass(constChecked);
					} else {
						$(aItem).removeClass(constChecked);
					}
				} else if (type === 'action') { // enable all except fullscreen on windows
					if (id === 'fullscreen' && (L.Browser.ie || L.Browser.edge)) { // Full screen works weirdly on IE 11 and on Edge
						$(aItem).addClass('disabled');
						var index = self.options.allowedViewModeActions.indexOf('fullscreen');
						if (index > 0) {
							self.options.allowedViewModeActions.splice(index, 1);
						}
					} else {
						$(aItem).removeClass('disabled');
					}
				}
			} else { // eslint-disable-next-line no-lonely-if
				if (type === 'unocommand') { // disable all uno commands
					$(aItem).addClass('disabled');
				} else if (type === 'action') { // disable all except allowedViewModeActions
					var found = false;
					for (var i in self.options.allowedViewModeActions) {
						if (self.options.allowedViewModeActions[i] === id) {
							found = true;
							break;
						}
					}
					if (!found) {
						$(aItem).addClass('disabled');
					} else {
						$(aItem).removeClass('disabled');
					}
				}
			}
		});
	},

	_executeAction: function(item) {
		var id = $(item).data('id');
		if (id === 'save') {
			this._map.save(true, true);
		} else if (id === 'saveas') {
			this._map.fire('postMessage', {msgId: 'UI_SaveAs'});
		} else if (id === 'shareas') {
			this._map.fire('postMessage', {msgId: 'UI_Share'});
		} else if (id === 'print') {
			this._map.print();
		} else if (id.startsWith('downloadas-')) {
			var format = id.substring('downloadas-'.length);
			var fileName = this._map['wopi'].BaseFileName;
			fileName = fileName.substr(0, fileName.lastIndexOf('.'));
			fileName = fileName === '' ? 'document' : fileName;
			this._map.downloadAs(fileName + '.' + format, format);
		} else if (id === 'signdocument') {
			this._map.showSignDocument();
		} else if (id === 'insertcomment') {
			this._map.insertComment();
		} else if (id === 'insertgraphic') {
			L.DomUtil.get('insertgraphic').click();
		} else if (id === 'insertgraphicremote') {
			this._map.fire('postMessage', {msgId: 'UI_InsertGraphic'});
		} else if (id === 'zoomin' && this._map.getZoom() < this._map.getMaxZoom()) {
			this._map.zoomIn(1);
		} else if (id === 'zoomout' && this._map.getZoom() > this._map.getMinZoom()) {
			this._map.zoomOut(1);
		} else if (id === 'zoomreset') {
			this._map.setZoom(this._map.options.zoom);
		} else if (id === 'fullscreen') {
			L.toggleFullScreen();
		} else if (id === 'fullscreen-presentation' && this._map.getDocType() === 'presentation') {
			this._map.fire('fullscreen');
		} else if (id === 'insertpage') {
			this._map.insertPage();
		} else if (id === 'duplicatepage') {
			this._map.duplicatePage();
		} else if (id === 'deletepage') {
			var map = this._map;
			vex.dialog.confirm({
				message: _('Are you sure you want to delete this slide?'),
				callback: function(e) {
					if (e) {
						map.deletePage();
					}
				}
			});
		} else if (id === 'about') {
			this._map.showLOAboutDialog();
		} else if (id === 'keyboard-shortcuts') {
			this._map.showLOKeyboardHelp();
		} else if (revHistoryEnabled && (id === 'rev-history' || id === 'last-mod')) {
			// if we are being loaded inside an iframe, ask
			// our host to show revision history mode
			this._map.fire('postMessage', {msgId: 'rev-history', args: {Deprecated: true}});
			this._map.fire('postMessage', {msgId: 'UI_FileVersions'});
		} else if (id === 'closedocument') {
			if (window.ThisIsAMobileApp) {
				window.webkit.messageHandlers.lool.postMessage('BYE', '*');
			} else {
				this._map.fire('postMessage', {msgId: 'close', args: {EverModified: this._map._everModified, Deprecated: true}});
				this._map.fire('postMessage', {msgId: 'UI_Close', args: {EverModified: this._map._everModified}});
			}
			this._map.remove();
		} else if (id === 'repair') {
			this._map._socket.sendMessage('commandvalues command=.uno:DocumentRepair');
		}
		// Inform the host if asked
		if ($(item).data('postmessage') === 'true') {
			this._map.fire('postMessage', {msgId: 'Clicked_Button', args: {Id: id} });
		}
	},

	_sendCommand: function (item) {
		var unoCommand = $(item).data('uno');
		if (unoCommand.startsWith('.uno:InsertPageHeader') || unoCommand.startsWith('.uno:InsertPageFooter')) {
			unoCommand = unoCommand + ($(item).hasClass('lo-menu-item-checked') ? 'On:bool=false' : 'On:bool=true');
		}
		this._map.sendUnoCommand(unoCommand);
	},

	_onItemSelected: function(e, item) {
		var self = e.data.self;
		var type = $(item).data('type');
		if (type === 'unocommand') {
			self._sendCommand(item);
		} else if (type === 'action') {
			self._executeAction(item);
		}

		if (!L.Browser.mobile && $(item).data('id') !== 'insertcomment')
			self._map.focus();
	},

	_onMouseEnter: function(e, item) {
		var self = e.data.self;
		var type = $(item).data('type');
		if (type === 'unocommand') {
			var unoCommand = $(item).data('uno');
			self._map.setHelpTarget(unoCommand);
		} else if (type === 'action') {
			var id = $(item).data('id');
			self._map.setHelpTarget('modules/online/menu/' + id);
		}
	},

	_onMouseLeave: function(e) {
		var self = e.data.self;
		self._map.setHelpTarget(null);
	},

	_onKeyDown: function(e) {
		var self = e.data.self;

		// handle help - F1
		if (e.type === 'keydown' && !e.shiftKey && !e.ctrlKey && !e.altKey && e.keyCode == 112) {
			self._map.showHelp();
		}
	},

	_createFileIcon: function() {
		var iconClass = 'document-logo';
		var docType = this._map.getDocType();
		if (docType === 'text') {
			iconClass += ' writer-icon-img';
		} else if (docType === 'spreadsheet') {
			iconClass += ' calc-icon-img';
		} else if (docType === 'presentation' || docType === 'drawing') {
			iconClass += ' impress-icon-img';
		}

		var liItem = L.DomUtil.create('li', '');
		liItem.id = 'document-header';
		var aItem = L.DomUtil.create('div', iconClass, liItem);
		$(aItem).data('id', 'document-logo');
		$(aItem).data('type', 'action');

		this._menubarCont.insertBefore(liItem, this._menubarCont.firstChild);

		var $docLogo = $(aItem);
		$docLogo.bind('click', {self: this}, this._createDocument);

	},

	_createMenu: function(menu) {
		var itemList = [];
		var docType = this._map.getDocType();
		for (var i in menu) {
			if (menu[i].id === 'about' && (L.DomUtil.get('about-dialog') === null)) {
				continue;
			}
			if (menu[i].id === 'signdocument' && (L.DomUtil.get('document-signing-bar') === null)) {
				continue;
			}

			if (this._map._permission === 'readonly' && menu[i].type === 'menu') {
				var found = false;
				for (var j in this.options.allowedReadonlyMenus) {
					if (this.options.allowedReadonlyMenus[j] === menu[i].id) {
						found = true;
						break;
					}
				}
				if (!found)
					continue;
			}

			if (this._map._permission === 'readonly' && menu[i].id === 'last-mod') {
				continue;
			}

			if (menu[i].type === 'action') {
				if ((menu[i].id === 'rev-history' && !revHistoryEnabled) ||
					(menu[i].id === 'closedocument' && !closebutton)) {
					continue;
				}
			}

			if (menu[i].id === 'print' && this._map['wopi'].HidePrintOption)
				continue;

			if (menu[i].id === 'save' && this._map['wopi'].HideSaveOption)
				continue;

			if (menu[i].id === 'saveas' && this._map['wopi'].UserCanNotWriteRelative)
				continue;

			if (menu[i].id === 'shareas' && !this._map['wopi'].EnableShare)
				continue;

			if (menu[i].id === 'insertgraphicremote' && !this._map['wopi'].EnableInsertRemoteImage)
				continue;

			if (menu[i].id && menu[i].id.startsWith('fullscreen-presentation') && this._map['wopi'].HideExportOption)
				continue;

			if (menu[i].id === 'changesmenu' && this._map['wopi'].HideChangeTrackingControls)
				continue;

			// Keep track of all 'downloadas-' options and register them as
			// export formats with docLayer which can then be publicly accessed unlike
			// this Menubar control for which there doesn't seem to be any easy way
			// to get access to.
			if (menu[i].id && menu[i].id.startsWith('downloadas-')) {
				var format = menu[i].id.substring('downloadas-'.length);
				this._map._docLayer.registerExportFormat(menu[i].name, format);

				if (this._map['wopi'].HideExportOption)
					continue;
			}

			var liItem = L.DomUtil.create('li', '');
			if (menu[i].id) {
				liItem.id = 'menu-' + menu[i].id;
				if (menu[i].id === 'closedocument' && this._map._permission === 'readonly') {
					// see corresponding css rule for readonly class usage
					L.DomUtil.addClass(liItem, 'readonly');
				}
			}
			var aItem = L.DomUtil.create('a', menu[i].disabled ? 'disabled' : '', liItem);
			if (menu[i].name !== undefined) {
				aItem.innerHTML = menu[i].name;
			} else if (menu[i].uno !== undefined) {
				aItem.innerHTML = _UNO(menu[i].uno, docType);
			} else {
				aItem.innerHTML = '';
			}
			// Add by Firefly
			// 有指定 Icon 再加
			if (menu[i].icon !== undefined) {
				L.DomUtil.create('i', 'menuicon ' + menu[i].icon, aItem);
			}
			if (menu[i].hotkey !== undefined) {
				var spanItem = L.DomUtil.create('span', 'hotkey', aItem);
				spanItem.innerHTML = menu[i].hotkey;
			}
			if (menu[i].type === 'menu') {
				var ulItem = L.DomUtil.create('ul', '', liItem);
				var subitemList = this._createMenu(menu[i].menu);
				if (!subitemList.length) {
					continue;
				}
				for (var idx in subitemList) {
					ulItem.appendChild(subitemList[idx]);
				}
			} else if (menu[i].type === 'unocommand' || menu[i].uno !== undefined) {
				$(aItem).data('type', 'unocommand');
				$(aItem).data('uno', menu[i].uno);
				$(aItem).data('tag', menu[i].tag);
			} else if (menu[i].type === 'separator') {
				$(aItem).addClass('separator');
			} else if (menu[i].type === 'action') {
				$(aItem).data('type', 'action');
				$(aItem).data('id', menu[i].id);
			}

			if (menu[i].tablet == false && window.mode.isTablet()) {
				$(aItem).css('display', 'none');
			}

			if (menu[i].mobile == false && window.mode.isMobile()) {
				$(aItem).css('display', 'none');
			}

			itemList.push(liItem);
		}

		return itemList;
	},

	_initializeMenu: function(menu) {
		var menuHtml = this._createMenu(menu);
		for (var i in menuHtml) {
			this._menubarCont.appendChild(menuHtml[i]);
		}
	}
});

L.control.menubar = function (options) {
	return new L.Control.Menubar(options);
};
