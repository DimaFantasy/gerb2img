object Form1: TForm1
  Left = 0
  Top = 0
  Caption = 'Form1'
  ClientHeight = 485
  ClientWidth = 843
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
    Top = 41
    Width = 843
    Height = 444
    Align = alClient
    AutoSize = True
    Proportional = True
    Stretch = True
    ExplicitLeft = 208
    ExplicitTop = 200
    ExplicitWidth = 105
    ExplicitHeight = 105
  end
  object Panel1: TPanel
    Left = 0
    Top = 0
    Width = 843
    Height = 41
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
  end
  object OpenDialog1: TOpenDialog
    Filter = 'Gerber|*.gbr'
    Left = 656
    Top = 336
  end
end
