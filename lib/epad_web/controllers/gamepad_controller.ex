defmodule EpadWeb.GamepadController do
  use EpadWeb, :controller

  plug :put_layout, "gamepad.html"

  def index(conn, _params) do
    render(conn, "index.html")
  end
end
