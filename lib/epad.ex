defmodule Epad do
  @moduledoc """
  Epad keeps the contexts that define your domain
  and business logic.

  Contexts are also responsible for managing your data, regardless
  if it comes from the database, an external API or others.
  """

  alias Epad.Hubs.Gamepad
  alias Epad.Hubs.Touchpad
  alias Epad.Hubs.Keyboard

  def gamepad(args \\ []) when is_list(args) do
    Keyword.merge(args, profile: :gamepad) |> Gamepad.start_child()
  end

  def touchpad(args) when is_list(args) do
    Keyword.merge(args, profile: :touchpad) |> Touchpad.start_child()
  end

  def touchpad(device_id) do
    touchpad(name: "touchpad#{device_id}" |> String.to_atom())
  end

  def keyboard(args) when is_list(args) do
    Keyword.merge(args, profile: :keyboard) |> Keyboard.start_child()
  end

  def keyboard(device_id) do
    keyboard(name: "keyboard-#{device_id}" |> String.to_atom())
  end
end
