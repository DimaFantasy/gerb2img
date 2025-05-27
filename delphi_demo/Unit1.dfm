object Form1: TForm1
  Left = 0
  Top = 0
  Caption = 'Form1'
  ClientHeight = 485
  ClientWidth = 904
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object Image1: TImage
    Left = 0
    Top = 193
    Width = 904
    Height = 292
    Align = alClient
    AutoSize = True
    Proportional = True
    Stretch = True
    ExplicitTop = 232
    ExplicitWidth = 843
    ExplicitHeight = 253
  end
  object Panel1: TPanel
    Left = 0
    Top = 0
    Width = 904
    Height = 193
    Align = alTop
    TabOrder = 0
    object ButtonprocessGerberJSON: TButton
      Left = 8
      Top = 7
      Width = 113
      Height = 25
      Caption = 'processGerberJSON'
      TabOrder = 0
      OnClick = ButtonprocessGerberJSONClick
    end
    object ButtonProcessGerber: TButton
      Left = 127
      Top = 7
      Width = 129
      Height = 25
      Caption = 'processGerber'
      TabOrder = 1
      OnClick = ButtonProcessGerberClick
    end
    object CheckBoxDebug: TCheckBox
      Left = 411
      Top = 11
      Width = 97
      Height = 17
      Caption = 'Debug dll ver'
      Checked = True
      State = cbChecked
      TabOrder = 2
    end
    object MaskEditDpi: TMaskEdit
      Left = 284
      Top = 9
      Width = 116
      Height = 21
      EditMask = '99999;0;_'
      MaxLength = 5
      TabOrder = 3
      Text = ''
    end
    object ButtonProcessExcellon: TButton
      Left = 633
      Top = 7
      Width = 113
      Height = 25
      Caption = 'ProcessExcellon'
      TabOrder = 4
      OnClick = ButtonProcessExcellonClick
    end
    object ButtonProcessExcellonJSON: TButton
      Left = 514
      Top = 7
      Width = 113
      Height = 25
      Caption = 'ProcessExcellonJSON'
      TabOrder = 5
      OnClick = ButtonProcessExcellonJSONClick
    end
    object GroupBox1: TGroupBox
      Left = 8
      Top = 38
      Width = 434
      Height = 147
      Caption = ' '#1053#1072#1089#1090#1088#1086#1081#1082#1080' Gerber '
      TabOrder = 6
      object GerberLabelScaleX: TLabel
        Left = 287
        Top = 12
        Width = 58
        Height = 13
        Caption = #1052#1072#1089#1096#1090#1072#1073' X:'
      end
      object GerberLabelScale: TLabel
        Left = 287
        Top = 35
        Width = 58
        Height = 13
        Caption = #1052#1072#1089#1096#1090#1072#1073' Y:'
      end
      object GerberCheckBoxInvertPolarity: TCheckBox
        Left = 16
        Top = 16
        Width = 65
        Height = 17
        Caption = #1053#1077#1075#1086#1090#1080#1074
        TabOrder = 0
      end
      object GerberEditScaleX: TEdit
        Left = 351
        Top = 9
        Width = 41
        Height = 21
        TabOrder = 1
        Text = '0'
      end
      object GerberEditScaleY: TEdit
        Left = 351
        Top = 32
        Width = 41
        Height = 21
        TabOrder = 2
        Text = '0'
      end
      object Edit1: TEdit
        Left = 127
        Top = 36
        Width = 121
        Height = 21
        TabOrder = 3
        Text = '0'
      end
      object GerberCheckBoxBorderMillimeters: TCheckBox
        Left = 16
        Top = 40
        Width = 105
        Height = 17
        Caption = #1054#1090#1089#1090#1091#1087' '#1074' '#1084#1084'/pix :'
        TabOrder = 4
      end
      object GerberEditGrowSize: TEdit
        Left = 199
        Top = 60
        Width = 121
        Height = 21
        TabOrder = 5
        Text = '0'
      end
      object GerberCheckBoxGrowMillimeters: TCheckBox
        Left = 16
        Top = 63
        Width = 177
        Height = 17
        Caption = #1056#1072#1089#1096#1080#1088#1077#1085#1080#1077' '#1087#1086#1083#1080#1075#1086#1085#1072' '#1074' '#1084#1084'/pix :'
        TabOrder = 6
      end
    end
    object GroupBox2: TGroupBox
      Left = 448
      Top = 38
      Width = 425
      Height = 147
      Caption = 'GroupBox2'
      TabOrder = 7
      object ExcellonLabelScaleX: TLabel
        Left = 16
        Top = 44
        Width = 58
        Height = 13
        Caption = #1052#1072#1089#1096#1090#1072#1073' X:'
      end
      object ExcellonLabelScaleY: TLabel
        Left = 16
        Top = 66
        Width = 58
        Height = 13
        Caption = #1052#1072#1089#1096#1090#1072#1073' Y:'
      end
      object ExcellonLabelBorder: TLabel
        Left = 11
        Top = 117
        Width = 44
        Height = 13
        Caption = #1054#1090#1089#1090#1091#1087' :'
      end
      object ExcellonLabelGrowSize: TLabel
        Left = 120
        Top = 115
        Width = 65
        Height = 13
        Caption = #1059#1074#1077#1083#1080#1095#1080#1090#1100' :'
      end
      object ExcellonCheckBoxInvertPolarity: TCheckBox
        Left = 16
        Top = 18
        Width = 73
        Height = 17
        Caption = #1053#1077#1075#1086#1090#1080#1074
        TabOrder = 0
      end
      object ExcellonEditScaleX: TEdit
        Left = 80
        Top = 41
        Width = 121
        Height = 21
        TabOrder = 1
        Text = '0'
      end
      object ExcellonEditScaleY: TEdit
        Left = 80
        Top = 63
        Width = 121
        Height = 21
        TabOrder = 2
        Text = '0'
      end
      object ExcellonCheckBoxUnitsMillimeters: TCheckBox
        Left = 10
        Top = 91
        Width = 223
        Height = 17
        Caption = #1045#1076#1080#1085#1080#1094#1099' '#1054#1090#1089#1090#1091#1087#1072', '#1059#1074#1077#1083#1080#1095#1077#1085#1080#1103', '#1084#1084'/pix'
        TabOrder = 3
      end
      object ExcellonEditBorder: TEdit
        Left = 57
        Top = 114
        Width = 36
        Height = 21
        TabOrder = 4
        Text = '0'
      end
      object ExcellonEditGrowSize: TEdit
        Left = 187
        Top = 112
        Width = 36
        Height = 21
        TabOrder = 5
        Text = '0'
        OnChange = ExcellonEditGrowSizeChange
      end
      object ExcellonCheckBoxUniformDrills: TCheckBox
        Left = 248
        Top = 20
        Width = 137
        Height = 17
        Caption = #1054#1076#1080#1085#1072#1082#1086#1074#1099#1077' '#1086#1090#1074#1077#1088#1089#1090#1080#1103
        TabOrder = 6
      end
      object ExcellonCheckBoxUniformDrillsMillimeters: TCheckBox
        Left = 248
        Top = 43
        Width = 161
        Height = 17
        Caption = #1044#1080#1072#1084#1077#1090#1088' '#1086#1090#1074#1077#1088#1089#1090#1080#1081' '#1074' '#1084#1084'/in'
        TabOrder = 7
      end
      object ExcellonEditUniformDrillDiameter: TEdit
        Left = 246
        Top = 66
        Width = 121
        Height = 21
        TabOrder = 8
        Text = '1'
      end
    end
  end
  object OpenDialog1: TOpenDialog
    Filter = 'Gerber|*.gbr|All|*.*'
    Left = 656
    Top = 336
  end
end
